#include <../include/PluginExtension.hpp>
#include <windows.h>

class MechShakerBridge final : public PluginExtension {
    struct EventData {
        int event_code;
        int int0;
        float float0;
        float float1;
        float float2;
        float float3;
        float float4;
        float float5;
    };

    struct ControlBlock {
        std::atomic<size_t> write_index = 0;
        std::atomic<size_t> packet_number = 0;
    };

    HANDLE map_file = nullptr;
    LPVOID buf = nullptr;
    ControlBlock* control_block = nullptr;
    int current_write_index = 0;

    const int buffer_size = 128;
    const size_t buffer_size_bytes = sizeof(EventData) * buffer_size;
    const size_t total_buffer_size_bytes = buffer_size_bytes + sizeof(ControlBlock);

    bool first_tick = true;

public:
    inline static MechShakerBridge* instance;

    MechShakerBridge() {
        instance = this;
        plugin_name = "MechShakerBridge";
    }

    ~MechShakerBridge() override {
        remove_all_event_hooks();
        constexpr auto close_event = EventData{-1, 0, 0, 0, 0, 0, 0, 0};
        write_to_shared_memory(&close_event);
        UnmapViewOfFile(buf);
        CloseHandle(map_file);
    }

    void on_pre_engine_tick(API::UGameEngine* engine, float delta) override {
        if (first_tick)
            on_first_tick();
    }

    static void* on_telemetry(API::UObject*, FFrame* frame, void* const) {
        instance->write_to_shared_memory(frame->get_params<EventData>());
        return nullptr;
    }

private:
    void on_first_tick() {
        first_tick = false;

        const auto player_controller = API::get()->get_player_controller(0);
        if (!player_controller) {
            log_error("Failed to get player controller");
            return;
        }

        API::UObject* controller_feedback_c;

        if (!try_get_property(player_controller, L"ControllerFeedbackComponent", controller_feedback_c))
            return;

        if (!add_event_hook(controller_feedback_c->get_class(), L"OnTelemetry", &on_telemetry)) {
            log_error("Failed to find OnTelemetry event, MechShakerRelay mod may not be installed - MechShaker will be disabled");
            return;
        }

        if (!setup_memory_mapped_file()) {
            remove_all_event_hooks_with_log();
        }
    }

    bool setup_memory_mapped_file() {
        log_info("Creating memory mapped file...");
        map_file =
            CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, total_buffer_size_bytes, TEXT("MechShakerBridgeMemory"));

        if (map_file == nullptr) {
            log_error("Could not create file mapping object: %i", GetLastError());
            return false;
        }

        buf = MapViewOfFile(map_file, FILE_MAP_ALL_ACCESS, 0, 0, total_buffer_size_bytes);

        if (buf == nullptr) {
            log_error("Could not map view of file: %i", GetLastError());
            CloseHandle(map_file);
            return false;
        }

        control_block = static_cast<ControlBlock*>(buf);

        log_info("Memory mapped file created");
        return true;
    }

    void write_to_shared_memory(const EventData* event_data) {
        // log_info("ec: %i, %i, %f, %f, %f, %f, %f, %f", event_data->event_code, event_data->int0, event_data->float0, event_data->float1,
        //     event_data->float2, event_data->float3, event_data->float4, event_data->float5);
        const size_t offset = sizeof(ControlBlock) + current_write_index * sizeof(EventData);
        current_write_index = (current_write_index + 1) % buffer_size;
        CopyMemory(static_cast<char*>(buf) + offset, event_data, sizeof(EventData));
        const size_t write_index = control_block->write_index.load(std::memory_order_acquire);
        const size_t packet_number = control_block->packet_number.load(std::memory_order_acquire);
        control_block->write_index.store(current_write_index, std::memory_order_release);
        control_block->packet_number.store(packet_number + 1, std::memory_order_release);
    }
};

std::unique_ptr<MechShakerBridge> g_plugin{new MechShakerBridge()};