# CAD

Source files, exported geometry and drawings for the structure.

| Folder | Contents |
|---|---|
| `source/` | Native CAD files. One assembly per subfolder |
| `step/` | STEP exports, which is what anyone without your CAD seat can open |
| `drawings/` | PDF drawings for anything being machined |

## Conventions

- Export a STEP alongside every change to a source file. A repository of
  native files alone is unusable to everyone but you.
- Name parts after what they are and what revision they are:
  `upper-link-r3.step`, not `part1_final_v2.step`.
- Keep drawings as PDF. Nobody should need your CAD package to read a
  dimension off a part they are quoting.
- Large binaries belong in Git LFS once this gets past a few megabytes. Set
  it up before the history is full of them, not after.

## Key dimensions

These are duplicated in `firmware/include/config.h`, which is what the
gravity model uses. If a link length changes here, change it there too, or
the feed-forward term quietly becomes wrong.

| Parameter | Value | Where it is used |
|---|---|---|
| Upper link length | 280 mm | `kLinkLength[0]` |
| Forearm link length | 255 mm | `kLinkLength[1]` |
| Upper link mass | 0.84 kg | `kLinkMass[0]` |
| Forearm link mass | 0.52 kg | `kLinkMass[1]` |

Masses include the share of the actuator and cuff carried by that link. Take
them from the CAD once the design settles; the values above are estimates.
