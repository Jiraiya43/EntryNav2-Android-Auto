# Roadmap

An item is checked off only once it has been verified in a car. Testing on a PC is not enough.

## 0.10: First public release

- [x] Read-only compatibility check (`aa-supervisor --compat`)
- [x] Manual installation procedure rehearsed with the car's BusyBox
- [ ] Follow the procedure end to end on the reference car
- [x] Test 0.10.0 in maintenance mode on the reference car (10 November 2026)
  - [ ] The check reports “Compatible,” including the dry-run startup
  - [x] Android Auto stays on screen after starting automatically when the phone reconnects, and the rotary controller
    doesn't affect iDrive
  - [ ] Media → Android Auto → *Ouvrir* (Open): iDrive stays on its blank page, even after selecting a driving mode and
    pressing *RETOUR* (Back)
  - [ ] Volume: the pill shows the volume level and Android Auto stays opaque
  - [ ] Android Auto closes about 5 seconds after Wi-Fi is turned off
  - [ ] *Arrêter* (Stop) and *Démarrer* (Start)
  - [ ] Rotary controller (one step per detent), buttons, touchscreen, driving modes, camera, and Assistant
  - [ ] Car GPS in Waze and arrows on the head-up display
- [ ] Everything works the same with logging off (no `maintenance` file)

## 0.12: USB cable

- [x] Android Auto starts over USB on the reference car, and wireless still works (11 November 2026)
- [ ] Drive for 20 to 30 minutes with USB connected and music playing over Bluetooth
- [ ] Unplug and reconnect the cable during the drive
- [ ] Stop and start without unplugging, then switch back to wireless
- [x] USB cable tested on other cars: X3 G01 ([#17](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/17)),
  i3 ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)), G21
  ([#26](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/26))

## 1.0: Another car

- [x] Compatibility report from a second car without installing anything: 2020 X3 G01
  ([#17](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/17))
- [ ] First installation on another car, followed by a verified uninstall
- [ ] Complete the table of tested software versions ([compatibility](docs/COMPATIBILITE.md))
- [x] Older rotary controller (C10/C20) tested in a real car: i3
  ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19), 11 November 2026)

<a name="limites-connues"></a>

## Known limitations

| Limitation | Cause | Possible solution |
|---|---|---|
| Audio uses the car's Bluetooth connection: quality is limited and the start of voice prompts can be cut off | No audio over Wi-Fi | Stream audio over Wi-Fi |
| When parking assistance opens without shifting into reverse, Android Auto stays in the background until *Ouvrir* (Open) is selected | The app doesn't know which screen iDrive is displaying | Read the PDC state |
| During a call, the rotary controller also affects the iDrive call screen | The app doesn't know which screen iDrive is displaying | Read which screen iDrive is displaying |
| On the head-up display, the arrow doesn't return after a speed limit is shown and stays hidden beyond 1.6 km | Instrument cluster behavior | Investigate |
| Roundabout exit text on the instrument cluster is in French | Hard-coded text | Follow the iDrive language |
| All installations share one vehicle identity, so a phone used in two equipped cars can't tell them apart | Fixed `vehicleId` | Give each installation its own ID |

## After 1.0

| Topic | Status |
|---|---|
| Audio over Wi-Fi instead of Bluetooth | On hold |
| Read directly which screen iDrive is displaying (alerts, PDC, calls) | To investigate |
| Send compass heading to the phone so the map stays oriented while stopped | Idea |
| Open Android Auto with a dashboard favorites button | Idea |
| Album art on iDrive | Idea |
| Other screen sizes | The size is read from the car; testing is underway on an i3 with an 800x480 display ([#19](https://github.com/Jiraiya43/EntryNav2-Android-Auto/issues/19)) |
| Support a different Bluetooth library layout | Declined for now: the report and library are missing |
| Show an on-screen message if Android Auto fails to start in production mode | Idea |
| Update without repeating the full installation procedure | Idea |

## Out of scope

- NBT Evo, ID7, and MGU: this program cannot run on those systems.
- Makes other than BMW.
