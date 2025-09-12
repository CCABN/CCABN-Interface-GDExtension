#include "register_types.h"
#include "video_stream.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void initialize_ccabn_videostream_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }

#ifdef DEBUG_ENABLED
    UtilityFunctions::print("CCABN VideoStream GDExtension: Initializing...");
#endif

    GDREGISTER_CLASS(VideoStream);

#ifdef DEBUG_ENABLED
    UtilityFunctions::print("CCABN VideoStream GDExtension: VideoStream class registered successfully!");
#endif
}

void uninitialize_ccabn_videostream_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}

extern "C" {
    GDExtensionBool GDE_EXPORT ccabn_videostream_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
#ifdef DEBUG_ENABLED
        UtilityFunctions::print("CCABN VideoStream GDExtension: Library loading...");
#endif

        godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

        init_obj.register_initializer(initialize_ccabn_videostream_module);
        init_obj.register_terminator(uninitialize_ccabn_videostream_module);
        init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

        GDExtensionBool result = init_obj.init();

#ifdef DEBUG_ENABLED
        if (result) {
            UtilityFunctions::print("CCABN VideoStream GDExtension: Library initialized successfully!");
        } else {
            UtilityFunctions::print("CCABN VideoStream GDExtension: Failed to initialize library!");
        }
#endif

        return result;
    }
}