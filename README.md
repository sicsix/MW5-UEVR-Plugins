# MW5-UEVR-Plugins

Several [UEVR](https://github.com/praydog/UEVR) plugins for the MechWarrior 5 mod [MechWarriorVR](https://www.nexusmods.com/mechwarrior5mercenaries/mods/1009).

### Head Aim
Handles head tracking and allows aiming arm mounted weapons with head movement.

### HUD
Injects a new manual rendering pass into the post process pipeline that replaces motion blur.
It intercepts the rendering of 3D HUD widgets in the normal rendering pipeline and draws them in the custom post process render pass instead.
This allows it to render after DLSS/TAA upscaling so it avoids any blurriness and smearing caused by those techniques.
It's also rendered before bloom so it gets a nice glow effect for free along with the rest of the scene.

### MechShakerBridge
The MechShakerRelay mod picks up in-game events, these are hooked by MechShakerBridge and written to a memory mapped file.
This MMF is read by the standalone [MechShaker](https://www.nexusmods.com/mechwarrior5mercenaries/mods/1029) software which outputs
audio signals to drive haptic transducers.

### Camera
A bit of a catch all but it contains a few camera/control/rendering related features.
- Hooks some Ground Truth Ambient Occlusion functions to fix two bugs in Unreal - one that causes the entire scene to darken instead of
doing anything useful as the depth and scene textures inputs were swapped, and another that causes an eye mismatch issue in VR as the second eye does not read from the HZB
texture correctly
- Hooks into some blueprint functions called by the MWVR mod to automatically place the projected 2D UI in the correct place for the starmap view
- Hooks the C++ function used by the game to set the active control method and overrides its behaviour to allow using VR controllers in the menus
- Hooks a bunch of other blueprint functions called by MWVR to control the camera position, mouse cursor visibility, and decoupled pitch state