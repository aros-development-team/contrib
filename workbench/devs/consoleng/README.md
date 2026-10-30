# ConsoleNG

ConsoleNG provides an AROS console device, a native Zune preferences editor and
regression probes. It integrates with the native console handler rather than
providing a separate terminal emulator.

See [Preferences](prefs/FEATURES.md), [retained-text navigation](NAVIGATION.md)
and [search](SEARCH.md) for configuration and shortcuts. The test directory
contains host checks and native AROS probes; host results do not establish native
rendering or hardware compatibility.

## Integration status

This Contrib export uses the contrib-workbench-devs-consoleng build target and
its preferences subtarget. The device and Preferences build from this layout against an existing x86_64
SMP ADT SDK. Preferences matches the accepted development binary exactly; the
device matches after stripping debug information and normalizing its generated
version date. This is not a fresh full-system build or physical-hardware test.
The development build uses companion core console-handler changes for borrowed
window integration, completion requesters and icon drops. Clipboard read replies
also require the matching ConClip companion. These core changes must be delivered
and validated separately; this module alone is not a complete system update.

The implementation supports a substantial terminal protocol subset, but does not
claim full Alacritty compatibility. Semantic delimiters have a Preferences editor and persisted profiles. Configurable
hints, integration with the OS multi-user defaults, and tab management remain
gaps. See the Preferences feature inventory for qualified behavior and open
acceptance boundaries.

## Building against an existing SDK

When bootstrapping only this module in a configured AROS build, run the setup
and quick targets sequentially. Quick targets assume their prerequisite SDK
and directory setup already exists.

```
make contrib-workbench-devs-consoleng-includes-dirs
make -j8 contrib-workbench-devs-consoleng-quick
make -j8 contrib-workbench-devs-consoleng-prefs-quick
make contrib-workbench-devs-consoleng-prefs-startup-script
make contrib-workbench-devs-consoleng-prefs-startup-package
```

Do not launch multiple top-level targets concurrently while mmake is generating
new makefiles. Parallel compilation within an initialized target is supported.

## Opening a console

Install the matching core handler and clipboard companions, then run:

```
Execute S:ConsoleNG-Shell
```

The launcher mounts `CONNG:` on demand using `S:ConsoleNG-Mountlist`, with a
64 KiB handler stack, then opens an ordinary native Shell. It does not mount a
console automatically during boot or replace the classic `CON:` device. An
already mounted `CONNG:` is reused; update any older mount definition to use the
same stack size before booting it.

Run `SYS:Prefs/ConsoleNG` for the editor. The package startup restores saved
future-console defaults through the OS's `ENV:`/`ENVARC:` assignments. A read-only
Live CD cannot save to a read-only `ENVARC:`; Use still applies session defaults.
