#include <PluginExtension.h>
#include <windows.h>

class MechShakerBridge final : public PluginExtension {
    struct EventData {
        int32_t EventCode;
        int32_t Int0;
        float   Float0;
        float   Float1;
        float   Float2;
        float   Float3;
        float   Float4;
        float   Float5;
    };

    struct ControlBlock {
        int64_t WriteIndex   = 0;
        int64_t PacketNumber = 0;
    };

    bool          Running           = false;
    HANDLE        MapFile           = nullptr;
    LPVOID        Buffer            = nullptr;
    ControlBlock* Control           = nullptr;
    int32_t       CurrentWriteIndex = 0;

    static constexpr int32_t BUFFER_SIZE             = 128;
    static constexpr size_t  BUFFER_SIZE_BYTES       = sizeof(EventData) * BUFFER_SIZE;
    static constexpr size_t  TOTAL_BUFFER_SIZE_BYTES = BUFFER_SIZE_BYTES + sizeof(ControlBlock);

    bool FirstTick = true;

public:
    inline static MechShakerBridge* Instance = nullptr;

    MechShakerBridge() {
        Instance      = this;
        PluginName    = "MechShakerBridge";
        PluginVersion = "2.0.4";
    }

    virtual ~MechShakerBridge() override {
        if (Running) {
            RemoveAllEventHooks(false);

            if (Buffer) {
                constexpr auto closeEvent = EventData{.EventCode = -1, .Int0 = 0, .Float0 = 0, .Float1 = 0, .Float2 = 0, .Float3 = 0, .Float4 = 0, .Float5 = 0};
                WriteToSharedMemory(&closeEvent);
                UnmapViewOfFile(Buffer);
                Buffer = nullptr;
            }

            if (MapFile) {
                CloseHandle(MapFile);
                MapFile = nullptr;
            }
        }

        Instance = nullptr;
    }

    virtual void on_pre_engine_tick(API::UGameEngine* engine, float delta) override {
        if (FirstTick)
            OnFirstTick();
    }

    static void* OnTelemetry(API::UObject*, FFrame* frame, void* const) {
        if (!Instance)
            return nullptr;

        Instance->WriteToSharedMemory(frame->GetParams<EventData>());
        return nullptr;
    }

private:
    void OnFirstTick() {
        FirstTick = false;

        const auto playerController = API::get()->get_player_controller(0);
        if (!playerController) {
            LogError("Failed to get player controller");
            return;
        }

        API::UObject* controllerFeedbackC = nullptr;

        if (!TryGetProperty(playerController, L"ControllerFeedbackComponent", controllerFeedbackC))
            return;

        if (!AddEventHook(controllerFeedbackC->get_class(), L"OnTelemetry", &OnTelemetry)) {
            LogError("Failed to find OnTelemetry event, MechShakerRelay mod may not be installed - MechShaker will be disabled");
            return;
        }

        if (!SetupMemoryMappedFile()) {
            RemoveAllEventHooks(true);
            return;
        }

        Running = true;
    }

    bool SetupMemoryMappedFile() {
        LogInfo("Creating memory mapped file...");
        MapFile = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, TOTAL_BUFFER_SIZE_BYTES, TEXT("MechShakerBridgeMemory"));

        if (MapFile == nullptr) {
            LogError("Could not create file mapping object: %i", GetLastError());
            return false;
        }

        Buffer = MapViewOfFile(MapFile, FILE_MAP_ALL_ACCESS, 0, 0, TOTAL_BUFFER_SIZE_BYTES);

        if (Buffer == nullptr) {
            LogError("Could not map view of file: %i", GetLastError());
            CloseHandle(MapFile);
            MapFile = nullptr;
            return false;
        }

        Control = static_cast<ControlBlock*>(Buffer);

        LogInfo("Memory mapped file created");
        return true;
    }

    void WriteToSharedMemory(const EventData* eventData) {
        if (!Running || !Buffer || !Control || !eventData)
            return;

        const auto   index  = CurrentWriteIndex;
        const size_t offset = sizeof(ControlBlock) + static_cast<size_t>(index) * sizeof(EventData);
        CurrentWriteIndex   = (CurrentWriteIndex + 1) % BUFFER_SIZE;

        CopyMemory(static_cast<char*>(Buffer) + offset, eventData, sizeof(EventData));

        Control->WriteIndex   = static_cast<int64_t>(index);
        Control->PacketNumber = Control->PacketNumber + 1;
    }
};

// ReSharper disable once CppInconsistentNaming
std::unique_ptr<MechShakerBridge> g_plugin{new MechShakerBridge()}; // NOLINT(misc-use-internal-linkage)
