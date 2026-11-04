"""Fusion 360 script: dump the design's geometry and export neutral CAD.

Run from Fusion with the exoskeleton design open:

    Utilities tab -> Scripts and Add-Ins -> Scripts -> ExoArmExport -> Run

It writes, into a folder you choose:

    design.json      parameters, components, joints, mass properties
    bodies.csv       one row per solid body
    <design>.step    STEP export of the whole design
    <design>.stl     mesh export of the whole design

Nothing in the design is modified. The script only reads and exports.

Fusion's API works in centimetres and kilograms internally. Everything
written out here is converted to millimetres and kilograms so it can go
straight into firmware/include/config.h.
"""

import json
import os
import traceback

import adsk.core
import adsk.fusion

CM_TO_MM = 10.0
CM3_TO_MM3 = 1000.0


def _point_mm(point):
    """Fusion Point3D (cm) -> dict in mm."""
    return {"x": point.x * CM_TO_MM, "y": point.y * CM_TO_MM, "z": point.z * CM_TO_MM}


def _physical(entity):
    """Mass properties at high accuracy, or None when the entity has none."""
    try:
        props = entity.getPhysicalProperties(
            adsk.fusion.CalculationAccuracy.HighCalculationAccuracy
        )
    except Exception:
        return None

    if props is None:
        return None

    return {
        "mass_kg": props.mass,
        "volume_mm3": props.volume * CM3_TO_MM3,
        "area_mm2": props.area * CM_TO_MM * CM_TO_MM,
        "centre_of_mass_mm": _point_mm(props.centerOfMass),
    }


def _bounding_box_mm(entity):
    try:
        box = entity.boundingBox
    except Exception:
        return None

    if box is None:
        return None

    return {
        "min_mm": _point_mm(box.minPoint),
        "max_mm": _point_mm(box.maxPoint),
        "size_mm": {
            "x": (box.maxPoint.x - box.minPoint.x) * CM_TO_MM,
            "y": (box.maxPoint.y - box.minPoint.y) * CM_TO_MM,
            "z": (box.maxPoint.z - box.minPoint.z) * CM_TO_MM,
        },
    }


def _parameters(design):
    out = []
    for param in design.allParameters:
        try:
            out.append(
                {
                    "name": param.name,
                    "expression": param.expression,
                    "value": param.value,
                    "unit": param.unit,
                    "comment": param.comment,
                }
            )
        except Exception:
            continue
    return out


def _material_name(body):
    try:
        return body.material.name if body.material else None
    except Exception:
        return None


def _bodies(component, occurrence_path):
    rows = []
    for body in component.bRepBodies:
        if not body.isSolid:
            continue
        rows.append(
            {
                "occurrence": occurrence_path,
                "component": component.name,
                "body": body.name,
                "material": _material_name(body),
                "visible": body.isVisible,
                "physical": _physical(body),
                "bounding_box": _bounding_box_mm(body),
            }
        )
    return rows


def _walk(occurrence, path, seen, components, bodies):
    """Depth-first walk of the occurrence tree."""
    component = occurrence.component
    here = path + "/" + occurrence.name

    if component.id not in seen:
        seen.add(component.id)
        components.append(
            {
                "name": component.name,
                "id": component.id,
                "description": component.description,
                "part_number": component.partNumber,
                "physical": _physical(component),
                "bounding_box": _bounding_box_mm(component),
                "body_count": component.bRepBodies.count,
            }
        )

    bodies.extend(_bodies(component, here))

    for child in occurrence.childOccurrences:
        _walk(child, here, seen, components, bodies)


JOINT_TYPE_NAMES = {}


def _joint_type_name(motion):
    if not JOINT_TYPE_NAMES:
        JOINT_TYPE_NAMES.update(
            {
                adsk.fusion.JointTypes.RigidJointType: "rigid",
                adsk.fusion.JointTypes.RevoluteJointType: "revolute",
                adsk.fusion.JointTypes.SliderJointType: "slider",
                adsk.fusion.JointTypes.CylindricalJointType: "cylindrical",
                adsk.fusion.JointTypes.PinSlotJointType: "pin_slot",
                adsk.fusion.JointTypes.PlanarJointType: "planar",
                adsk.fusion.JointTypes.BallJointType: "ball",
            }
        )
    return JOINT_TYPE_NAMES.get(motion.jointType, str(motion.jointType))


def _joints(root):
    out = []
    for joint in root.joints:
        entry = {
            "name": joint.name,
            "type": None,
            "origin_mm": None,
            "suppressed": joint.isSuppressed,
        }

        try:
            motion = joint.jointMotion
            entry["type"] = _joint_type_name(motion)

            if entry["type"] == "revolute":
                limits = motion.rotationLimits
                if limits and limits.isMaximumValueEnabled and limits.isMinimumValueEnabled:
                    entry["rotation_limits_rad"] = {
                        "min": limits.minimumValue,
                        "max": limits.maximumValue,
                    }
        except Exception:
            pass

        try:
            geometry = joint.geometryOrOriginOne
            if geometry and geometry.origin:
                entry["origin_mm"] = _point_mm(geometry.origin)
        except Exception:
            pass

        out.append(entry)
    return out


def _export(design, folder, stem):
    """STEP and STL of the whole design. Returns what was written."""
    manager = design.exportManager
    written = {}

    step_path = os.path.join(folder, stem + ".step")
    try:
        options = manager.createSTEPExportOptions(step_path, design.rootComponent)
        manager.execute(options)
        written["step"] = step_path
    except Exception as exc:
        written["step_error"] = str(exc)

    stl_path = os.path.join(folder, stem + ".stl")
    try:
        options = manager.createSTLExportOptions(design.rootComponent, stl_path)
        options.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementMedium
        manager.execute(options)
        written["stl"] = stl_path
    except Exception as exc:
        written["stl_error"] = str(exc)

    return written


CSV_COLUMNS = [
    "occurrence",
    "component",
    "body",
    "material",
    "mass_kg",
    "volume_mm3",
    "com_x_mm",
    "com_y_mm",
    "com_z_mm",
    "size_x_mm",
    "size_y_mm",
    "size_z_mm",
]


def _cell(value):
    if value is None:
        return ""
    text = str(value)
    if "," in text or chr(34) in text:
        return chr(34) + text.replace(chr(34), chr(34) * 2) + chr(34)
    return text


def _write_csv(path, bodies):
    lines = [",".join(CSV_COLUMNS)]

    for row in bodies:
        physical = row.get("physical") or {}
        com = physical.get("centre_of_mass_mm") or {}
        size = (row.get("bounding_box") or {}).get("size_mm") or {}

        values = (
            row.get("occurrence"),
            row.get("component"),
            row.get("body"),
            row.get("material"),
            physical.get("mass_kg"),
            physical.get("volume_mm3"),
            com.get("x"),
            com.get("y"),
            com.get("z"),
            size.get("x"),
            size.get("y"),
            size.get("z"),
        )
        lines.append(",".join(_cell(v) for v in values))

    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("\n".join(lines) + "\n")


def run(_context):
    app = adsk.core.Application.get()
    ui = app.userInterface

    try:
        design = adsk.fusion.Design.cast(app.activeProduct)
        if design is None:
            ui.messageBox("Open the exoskeleton design first, then run this script.")
            return

        dialog = ui.createFolderDialog()
        dialog.title = "Where should the export go?"
        if dialog.showDialog() != adsk.core.DialogResults.DialogOK:
            return
        folder = dialog.folder

        root = design.rootComponent
        stem = (root.name or "design").replace(" ", "_")

        components = []
        bodies = []
        seen = set()

        components.append(
            {
                "name": root.name,
                "id": root.id,
                "description": root.description,
                "part_number": root.partNumber,
                "physical": _physical(root),
                "bounding_box": _bounding_box_mm(root),
                "body_count": root.bRepBodies.count,
                "is_root": True,
            }
        )
        bodies.extend(_bodies(root, ""))

        for occurrence in root.occurrences:
            _walk(occurrence, "", seen, components, bodies)

        payload = {
            "design_name": root.name,
            "units": {"length": "mm", "mass": "kg", "angle": "rad"},
            "fusion_default_length_unit": design.unitsManager.defaultLengthUnits,
            "parameters": _parameters(design),
            "components": components,
            "bodies": bodies,
            "joints": _joints(root),
            "exports": _export(design, folder, stem),
        }

        with open(os.path.join(folder, "design.json"), "w", encoding="utf-8") as handle:
            json.dump(payload, handle, indent=2)

        _write_csv(os.path.join(folder, "bodies.csv"), bodies)

        total = 0.0
        for body in bodies:
            physical = body.get("physical") or {}
            total += physical.get("mass_kg") or 0.0

        ui.messageBox(
            "Exported {0} components, {1} solid bodies, {2} joints.\n"
            "Total mass: {3:.3f} kg\n\nWritten to:\n{4}".format(
                len(components), len(bodies), len(payload["joints"]), total, folder
            )
        )

    except Exception:
        if ui:
            ui.messageBox("ExoArmExport failed:\n{0}".format(traceback.format_exc()))
