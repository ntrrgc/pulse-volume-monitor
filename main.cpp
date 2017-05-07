#include <iostream>
#include <string>
#include <unicode/unistr.h>
#include <unicode/ustream.h>
#include <pulse/pulseaudio.h>
#include <unistd.h>
#include <tclap/CmdLine.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#undef NDEBUG // Enable assertions in production for this file

using namespace std;
using icu::UnicodeString;

pa_mainloop * mainloop = NULL;
pa_context *pa = NULL;
uint32_t sink_index = -1;

bool verbose = true;
bool use_json = false;
UnicodeString desired_sink_name = "";


enum MAINLOOP_QUIT_REASON{
    QUIT_CTRL_C = 0,
    QUIT_ERROR = 1,
    QUIT_RETRY_CONNECT = 2,
};

// Boring forward declarations in no particular order.
void sink_info_list_received(pa_context *c, const pa_sink_info *i, int eol,
                             void *userdata);
void report_volume();
void start_monitoring_volume();
void choose_desired_sink();
void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t index,
                  void *userdata);
void got_sink_info(pa_context *c, const pa_sink_info *i, int eol,
                   void *userdata);
void context_state_callback(pa_context *c, void *userdata);
void interrupt_loop(int signal);


// Reading callback-baesd asynchronous code is a bit tough, so here there are,
// the functions defined in the order they are executed once a connection to
// PulseAudio is requested.

void report_volume_text(const pa_sink_info *i) ;

void report_volume_json(const pa_sink_info *i) ;

int connect_to_pulseaudio() {
    int ret;

    // Use the "simple" mainloop API
    mainloop = pa_mainloop_new();
    pa_mainloop_api* mainloop_api = pa_mainloop_get_api(mainloop);
    assert(mainloop_api);

    // Create a PA context
    pa = pa_context_new(mainloop_api, NULL);
    if (pa == NULL) {
        cerr << "Could not create PulseAudio context.\n";
        return 1;
    }

    // Set the state callback to be notified when the connection succeeds.
    pa_context_set_state_callback(pa, context_state_callback, NULL);

    ret = pa_context_connect(pa, NULL, PA_CONTEXT_NOFLAGS, NULL);
    if (ret < 0) {
        cerr << "Failed to connect.\n";
        return QUIT_RETRY_CONNECT;
    }

    signal(SIGINT, interrupt_loop);

    // The mainloop can return a value, just like UNIX processes
    int process_ret;
    pa_mainloop_run(mainloop, &process_ret);

    return process_ret;
}

// On ^C quit the mainloop properly. This is mostly just to check leaks in
// Valgrind.
void interrupt_loop(int signal) {
    pa_mainloop_quit(mainloop, QUIT_CTRL_C);
}

void context_state_callback(pa_context *c, void *userdata) {
    assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            // Executed once when the connection succeeds
            choose_desired_sink();

            break;
        }

        case PA_CONTEXT_FAILED:
            cerr << "Connection failed.\n";
            pa_mainloop_quit(mainloop, QUIT_RETRY_CONNECT);
            return;

        case PA_CONTEXT_TERMINATED:
        default:
            pa_mainloop_quit(mainloop, QUIT_ERROR);
            return;
    }
}

// The first step is finding the desired sink (audio card). We ask pulse for
// the list of sinks.
void choose_desired_sink() {
    pa_operation * op = pa_context_get_sink_info_list(pa,
           sink_info_list_received, NULL);
    pa_operation_unref(op);
}

// For each sink this function is called.
void sink_info_list_received(pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
    // eol is -1 in case of error. Otherwise, it is 0 for the first sink, 1 for
    // the second and so on. The sink information is in the variable i.
    // When there are no more sinks pulseaudio makes a call with i == NULL.

    if (eol >= 0 && i) {
        // Found a sink.
        if (verbose) {
            cerr << "Found sink:\n"
            << "  Name: " << i->name << "\n"
            << "  Description: " << i->description << "\n";
        }

        // Have we found the desired sink yet?
        if (sink_index == -1) {
            // We haven't. Check if this sink is appropriate.

            UnicodeString name(i->name, "UTF-8");
            UnicodeString description(i->description, "UTF-8");
            name.toLower();
            description.toLower();

            if (desired_sink_name.length() == 0) {
                // No filter, we just pick the first sink.
                sink_index = i->index;
                if (verbose) {
                    cerr << "  Picking this one for being the first sink.\n";
                }
            } else if (name.indexOf(desired_sink_name) != -1) {
                // Raw name contains the match string
                sink_index = i->index;
                if (verbose) {
                    cerr << "  Picking this one for matching the name.\n";
                }
            } else if (description.indexOf(desired_sink_name) != -1) {
                // Description contains the match string
                sink_index = i->index;
                if (verbose) {
                    cerr << "  Picking this one for matching the description"
                            ".\n";
                }
            }
        }
    } else if (eol >= 0 && !i) {
        // There are no more sinks. Have one been chosen?
        if (sink_index != -1) {
            // Yes, proceed to next step.
            start_monitoring_volume();
        } else {
            // No, quit...
            cerr << "Could not found a valid sink.\n";
            pa_mainloop_quit(mainloop, QUIT_ERROR);
        }
    } else if (eol < 1) {
        cerr << "Error getting sink information.\n";
    }
}

// Next step, we report the current volume level and set up a hook to be
// notified on future changes.
void start_monitoring_volume() {
    report_volume();

    pa_context_set_subscribe_callback(pa, subscribe_cb, NULL);
    pa_operation *op = pa_context_subscribe(pa, (pa_subscription_mask_t)
            (PA_SUBSCRIPTION_MASK_SINK), NULL, NULL);
    assert(op);
    pa_operation_unref(op);
}

// Reporting the volume requires a request to pulseaudio.
void report_volume() {
    pa_operation * op = pa_context_get_sink_info_by_index(pa, sink_index,
                                                          got_sink_info, NULL);
    pa_operation_unref(op);
}

// This function contains the response to the previous request.
void got_sink_info(pa_context *c, const pa_sink_info *i, int eol,
                   void *userdata)
{
    // We should only get two calls, like this:
    //  (eol=0, i=[sink data])
    //  (eol=1, i=NULL)
    // If the sink did not exist we would only get one call, like this:
    //  (eol=0, i=NULL)

    if (eol == 0) {
        if (i) {

            if (use_json) {
                report_volume_json(i);
            } else {
                report_volume_text(i);
            }
            cout.flush();

        } else {
            cerr << "Sink " << sink_index << " does not exist. Quitting...\n";
            pa_mainloop_quit(mainloop, QUIT_ERROR);
        }
    } else if (eol < 1) {
        cerr << "Error getting sink information.\n";
    }
}

uint32_t get_max_volume(const pa_sink_info *i) {
    uint32_t max = 0;
    for (auto j = 0; j < i->volume.channels; ++j) {
        uint32_t volume = i->volume.values[j];
        if (volume > max) {
            max = volume;
        }
    }
    return max;
}

void report_volume_text(const pa_sink_info *i) {
    if (i->mute) {
        cout << "muted\n";
    } else {
        // Use volume of the channel with the greatest value
        uint32_t volume = get_max_volume(i);
        cout << 100. * float(volume) / PA_VOLUME_NORM << "%\n";
    }
}

void report_volume_json(const pa_sink_info *i) {
    using namespace rapidjson;

    Document d;
    d.SetObject();

    Document::AllocatorType& alloc = d.GetAllocator();

    d.AddMember("muted", Value((bool) i->mute), alloc);
    d.AddMember("max_volume", Value(double(get_max_volume(i)) / PA_VOLUME_NORM),
                alloc);

    Value channels(kArrayType);
    for (auto j = 0; j < i->volume.channels; ++j) {
        double volume = double(i->volume.values[j]) / PA_VOLUME_NORM;
        channels.PushBack(Value(volume), alloc);
    }
    d.AddMember("channels", channels, alloc);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    d.Accept(writer);

    cout << buffer.GetString() << "\n";
}

// Since pa_context_subscribe was called, this function is executed every time
// a sink state changes. We update the volume report then.
void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t index,
                  void *userdata)
{
    report_volume();
}

int main(int argc, char ** const argv) {
    using namespace TCLAP;

    try {
        CmdLine cmd("Reports the current volume of a PulseAudio sink, in an "
                            "asynchronous way (without polling)");

        SwitchArg verbose_switch("v", "verbose", "Shows more information than "
                "really necessary.", cmd);
        SwitchArg json_switch("j", "json", "Use detailed JSON output.", cmd);
        ValueArg<string> desired_sink_arg("s", "desired-sink", "Insert here "
                "part of the name of an audio card (as seen in "
                "pavucontrol) and it will be selected instead of just "
                "picking the first available.", false, "", "name", cmd);

        cmd.parse(argc, argv);

        verbose = verbose_switch.getValue();
        desired_sink_name = UnicodeString::fromUTF8(desired_sink_arg.getValue())
                .toLower();
        use_json = json_switch.getValue();
    } catch (ArgException &e) {
        cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
        return 1;
    }

    int ret;
    while (true) {
        ret = connect_to_pulseaudio();

        // Tear down PulseAudio context
        if (pa) {
            pa_context_unref(pa);
            pa = NULL;
        }

        if (ret == QUIT_RETRY_CONNECT) {
            cerr << "Retrying...\n";
            sleep(3);
        } else {
            break;
        }
    }

    return ret;
}
