"""Builds the Neocortex sample maps and imports Cora, headlessly.

Run it from your project (the Python Editor Script Plugin must be enabled):

    UnrealEditor-Cmd.exe YourProject.uproject -ExecutePythonScript="<plugin>/Scripts/build_samples.py" -unattended -nullrhi

It imports Extras/Cora into /Neocortex/Samples/Cora, makes the checker floor material, and writes
one map per sample, plus the launcher, to /Neocortex/Samples/Maps. Each map holds its scene as
actors (the characters, the camera, the shapes) and shows its screen, a Widget Blueprint next to
the map named WBP_ and the map name. Running it again replaces what it made before.
"""

import os
import unreal

PLUGIN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CORA_FBX = os.path.join(PLUGIN_DIR, "Extras", "Cora", "cora.fbx")
CORA_ANIMATIONS_FBX = os.path.join(PLUGIN_DIR, "Extras", "Cora", "cora_animations.fbx")
UI_PNG_DIR = os.path.join(PLUGIN_DIR, "Extras", "UI")
CORA_DIR = "/Neocortex/Samples/Cora"
MAPS_DIR = "/Neocortex/Samples/Maps"
CHECKER_TEXTURE = "/Neocortex/Samples/T_Checker"
CHECKER_MATERIAL = "/Neocortex/Samples/M_Checker"

# Map name, sample actor class, whether it has a floor. The others stand on black.
MAPS = [
    ("NeocortexSamples", "NeocortexSampleLauncher", False),
    ("1_TextChat", "NeocortexTextChatSample", False),
    ("2_AudioChat", "NeocortexAudioChatSample", False),
    ("3_Actions", "NeocortexActionsSample", False),
    ("4_Interactables", "NeocortexInteractablesSample", True),
    ("5_GroupChat", "NeocortexGroupChatSample", True),
    ("6_UsageGating", "NeocortexUsageGatingSample", False),
    ("7_QuizShow", "NeocortexQuizShowSample", False),
]


def log(text):
    unreal.log("[Neocortex samples] " + text)


def import_cora():
    if not os.path.exists(CORA_FBX):
        unreal.log_error("[Neocortex samples] Missing " + CORA_FBX)
        return

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.import_animations = False
    options.import_materials = True
    options.import_textures = True
    options.create_physics_asset = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.skeletal_mesh_import_data.set_editor_property("import_morph_targets", True)

    task = unreal.AssetImportTask()
    task.filename = CORA_FBX
    task.destination_path = CORA_DIR
    task.destination_name = "cora"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log("Imported Cora: " + ", ".join(task.imported_object_paths))

    import_animations()
    paint_cora()
    # The importer can bring the textures in next to the mesh as well; the copies in Textures are the ones in use.
    for path in unreal.EditorAssetLibrary.list_assets(CORA_DIR, recursive=False):
        if os.path.basename(path).startswith("T_M_"):
            unreal.EditorAssetLibrary.delete_asset(path)
    # The import saves the mesh only; the skeleton, materials and textures are saved here.
    unreal.EditorAssetLibrary.save_directory(CORA_DIR)


def import_animations():
    """Imports Cora's clips (idle, talking, thinking, walking, dance, jump) onto her skeleton."""
    skeleton = unreal.load_asset(CORA_DIR + "/cora_Skeleton")
    if skeleton is None or not os.path.exists(CORA_ANIMATIONS_FBX):
        unreal.log_warning("[Neocortex samples] No skeleton or no animation file; Cora will stand still.")
        return

    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.anim_sequence_import_data.set_editor_property("import_bone_tracks", True)
    options.anim_sequence_import_data.set_editor_property("remove_redundant_keys", False)

    task = unreal.AssetImportTask()
    task.filename = CORA_ANIMATIONS_FBX
    task.destination_path = CORA_DIR + "/Animations"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    if unreal.EditorAssetLibrary.does_directory_exist(task.destination_path):
        unreal.EditorAssetLibrary.delete_directory(task.destination_path)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    # Each take arrives as "<file>_<armature>_<take>"; the samples look them up by take name.
    for path in unreal.EditorAssetLibrary.list_assets(task.destination_path, recursive=False):
        take = os.path.splitext(os.path.basename(path))[0].split("_")[-1]
        target = task.destination_path + "/" + take.capitalize()
        if path.split(".")[0] != target:
            unreal.EditorAssetLibrary.rename_asset(path.split(".")[0], target)

    # The clips hold the hair in a fixed pose from a differently oriented hair rig, which pulls the
    # hair off the head. Without those tracks the hair keeps the model's own pose and follows the head.
    for path in unreal.EditorAssetLibrary.list_assets(task.destination_path, recursive=False):
        clip = unreal.load_asset(path.split(".")[0])
        unreal.AnimationLibrary.remove_bone_animation(clip, "HairRoot", True, True)
        unreal.EditorAssetLibrary.save_loaded_asset(clip)
    centre_on_feet(task.destination_path)
    names = [os.path.basename(p).split(".")[0] for p in unreal.EditorAssetLibrary.list_assets(task.destination_path, recursive=False)]
    log("Imported animations: " + ", ".join(names))


# Clips she stands in. As authored, each stands 10 to 15 cm to one side of the character's
# origin, so she stops beside whatever she walks to.
CENTRE_ON_FEET = ("Idle", "Talking", "Thinking")
FEET = ("LeftFoot", "RightFoot", "LeftToeBase", "RightToeBase")


def centre_on_feet(folder):
    """Moves each standing clip so the average point between her feet is on the origin.

    A constant shift of the hips, so the body still sways over planted feet as authored.
    """
    options = unreal.AnimPoseEvaluationOptions()
    for name in CENTRE_ON_FEET:
        clip = unreal.load_asset(folder + "/" + name)
        if clip is None:
            continue
        frames = unreal.AnimationLibrary.get_num_frames(clip)
        total = unreal.Vector(0, 0, 0)
        count = 0
        for frame in range(frames + 1):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_frame(clip, frame, options)
            for foot in FEET:
                total += unreal.AnimPoseExtensions.get_bone_pose(pose, foot, unreal.AnimPoseSpaces.WORLD).translation
                count += 1
        if count == 0:
            continue
        offset = total / count
        offset.z = 0.0

        # The hips are the root bone, so their own space is the model's.
        positions, rotations, scales = [], [], []
        for frame in range(frames + 1):
            hips = unreal.AnimationLibrary.get_bone_pose_for_frame(clip, "Hips", frame, False)
            positions.append(hips.translation - offset)
            rotations.append(hips.rotation)
            scales.append(hips.scale3d)
        clip.controller.set_bone_track_keys("Hips", positions, rotations, scales)
        unreal.EditorAssetLibrary.save_loaded_asset(clip)
        log("%s: moved %.1f cm to centre her feet" % (name, offset.length()))


def import_checker():
    """Imports Extras/UI/checker.png for the floors: sharp squares, with mips for the distance."""
    if unreal.EditorAssetLibrary.does_asset_exist(CHECKER_TEXTURE):
        unreal.EditorAssetLibrary.delete_asset(CHECKER_TEXTURE)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(UI_PNG_DIR, "checker.png")
    task.destination_path = CHECKER_TEXTURE.rsplit("/", 1)[0]
    task.destination_name = CHECKER_TEXTURE.rsplit("/", 1)[1]
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(CHECKER_TEXTURE)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    unreal.EditorAssetLibrary.save_asset(CHECKER_TEXTURE)


def make_checker_material():
    """A grey checker for the floors, the checker texture tiled across the plane."""
    if unreal.EditorAssetLibrary.does_asset_exist(CHECKER_MATERIAL):
        unreal.EditorAssetLibrary.delete_asset(CHECKER_MATERIAL)
    folder, name = CHECKER_MATERIAL.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -700, 0)
    uv.set_editor_property("u_tiling", 120.0)
    uv.set_editor_property("v_tiling", 120.0)
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 0)
    sample.set_editor_property("texture", unreal.load_asset(CHECKER_TEXTURE))
    lib.connect_material_expressions(uv, "", sample, "UVs")
    lib.connect_material_property(sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.9)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(CHECKER_MATERIAL)


def import_texture(path, is_data):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = CORA_DIR + "/Textures"
    task.automated = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(task.imported_object_paths[0]) if task.imported_object_paths else None
    if texture and is_data:
        texture.set_editor_property("srgb", False)
    return texture


def paint_cora():
    """Wires Cora's textures into the materials the import made (M_Body, M_Head, M_Cloth)."""
    textures_dir = os.path.join(os.path.dirname(CORA_FBX), "Textures")
    lib = unreal.MaterialEditingLibrary
    for part in ("Body", "Head", "Cloth"):
        material = unreal.load_asset(CORA_DIR + "/M_" + part)
        if material is None:
            unreal.log_warning("[Neocortex samples] No material M_" + part)
            continue

        base = import_texture(os.path.join(textures_dir, "T_M_%s_BC.jpg" % part), False)
        normal = import_texture(os.path.join(textures_dir, "T_M_%s_N.jpg" % part), True)
        rough = import_texture(os.path.join(textures_dir, "T_M_%s_Roughness.jpg" % part), True)
        if normal:
            normal.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)

        lib.delete_all_material_expressions(material)
        material.set_editor_property("used_with_skeletal_mesh", True)
        material.set_editor_property("used_with_morph_targets", True)

        def sample(texture, y, sampler, prop, output=""):
            if texture is None:
                return
            node = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, y)
            node.set_editor_property("texture", texture)
            node.set_editor_property("sampler_type", sampler)
            lib.connect_material_property(node, output, prop)

        sample(base, -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialProperty.MP_BASE_COLOR, "RGB")
        sample(normal, 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialProperty.MP_NORMAL, "RGB")
        sample(rough, 200, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR, unreal.MaterialProperty.MP_ROUGHNESS, "R")
        lib.recompile_material(material)
        log("Painted M_" + part)


def spawn(actor_class, location=(0, 0, 0), rotation=(0, 0, 0)):
    return unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class, unreal.Vector(*location), unreal.Rotator(*rotation))


# Where things stand, in centimetres. The characters face +X, toward the camera.
CORA_SPOT = (0, 30, 0)
FRONT_CAMERA = ((200, 0, 150), (0, -15, 180))
HIGH_CAMERA = ((300, 300, 200), (0, -20, -135))

# Per sample: the camera, the characters (class, location, yaw, launcher pick) and any shapes
# (shape, colour name, sRGB colour, location).
SCENES = {
    "NeocortexSampleLauncher": {},
    "NeocortexTextChatSample": {"camera": FRONT_CAMERA, "characters": [("NeocortexSampleCharacter", CORA_SPOT, 0, 0)], "history": True},
    "NeocortexAudioChatSample": {"camera": FRONT_CAMERA, "characters": [("NeocortexSampleCharacter", CORA_SPOT, 0, 0)], "history": True},
    "NeocortexActionsSample": {"camera": FRONT_CAMERA, "characters": [("NeocortexSampleCharacter", CORA_SPOT, 0, 0)]},
    "NeocortexInteractablesSample": {
        "camera": HIGH_CAMERA,
        "characters": [("NeocortexSampleCharacter", CORA_SPOT, 0, 0)],
        "shapes": [
            ("SQUARE", "red", (154, 0, 0), (-200, -300, 0)),
            ("CIRCLE", "yellow", (255, 185, 0), (54, 224, 0)),
            ("SQUARE", "blue", (0, 50, 154), (200, 50, 0)),
        ],
    },
    "NeocortexGroupChatSample": {
        "camera": HIGH_CAMERA,
        "characters": [
            ("NeocortexSampleCharacter", (120, 120, 0), 0, 0),
            ("NeocortexSampleCharacter", (50, 200, 0), 0, 1),
            ("NeocortexSampleCharacter", (0, -200, 0), 0, 2),
        ],
        "seats": [(50, 200, 0), (200, 50, 0)],
        "offstage": [(-200, 0, 0), (0, -200, 0)],
    },
    "NeocortexUsageGatingSample": {"camera": FRONT_CAMERA, "characters": [("NeocortexSampleCharacter", CORA_SPOT, 0, 0)], "history": True, "text_only": True},
    "NeocortexQuizShowSample": {"camera": ((130, -80, 150), (0, -3, 180)), "characters": [("NeocortexSampleQuizHost", CORA_SPOT, -15, 0)]},
}


def build_screen(map_name, sample_class):
    """Writes the sample's screen into WBP_<map> next to the map, from the same layout code."""
    path = MAPS_DIR + "/WBP_" + map_name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    screen = unreal.NeocortexSampleAssetTools.build_sample_screen(path, sample_class)
    if screen is None:
        unreal.log_error("[Neocortex samples] Could not build " + path)
        return None
    return unreal.load_class(None, path + "." + "WBP_" + map_name + "_C")


def build_scene(map_name, sample_class_name, has_floor):
    if has_floor:
        floor = spawn(unreal.StaticMeshActor, (0, 0, 0))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
        floor.static_mesh_component.set_material(0, unreal.load_asset(CHECKER_MATERIAL))
        floor.set_actor_scale3d(unreal.Vector(40, 40, 1))
        # Lit live like the rest of the scene, so there is no lighting to bake.
        floor.static_mesh_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        floor.set_actor_label("Floor")

    # Roll, pitch, yaw. One key light from the front right casts the shadows, and a soft ambient
    # light keeps the unlit side from going black.
    key = spawn(unreal.DirectionalLight, (0, 0, 500), (0, -40, 145))
    key.light_component.set_intensity(4.0)
    key.set_actor_label("KeyLight")
    ambient = spawn(unreal.SkyLight, (0, 0, 300))
    ambient.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    cubemap = unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")
    if cubemap:
        ambient.light_component.set_editor_property("cubemap", cubemap)
    ambient.light_component.set_intensity(1.0)
    # Every light is live, so the maps never need their lighting built.
    for light in (key, ambient):
        light.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    ambient.set_actor_label("Ambient")

    # A fixed exposure, so the black backdrop does not brighten the scene.
    post = spawn(unreal.PostProcessVolume, (0, 0, 0))
    post.set_editor_property("unbound", True)
    settings = post.get_editor_property("settings")
    settings.set_editor_property("override_auto_exposure_method", True)
    settings.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
    settings.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
    settings.set_editor_property("override_auto_exposure_bias", True)
    settings.set_editor_property("auto_exposure_bias", 0.0)
    post.set_editor_property("settings", settings)
    post.set_actor_label("Exposure")

    spawn(unreal.PlayerStart, (500, 0, 100), (0, 0, 180)).set_actor_label("PlayerStart")

    sample_class = unreal.load_class(None, "/Script/Neocortex." + sample_class_name)
    if sample_class is None:
        unreal.log_error("[Neocortex samples] No class " + sample_class_name)
        return
    sample = spawn(sample_class, (0, 0, 0))
    sample.set_actor_label("Sample")
    scene = SCENES.get(sample_class_name, {})

    # The screen, as a Widget Blueprint to open in the UMG designer.
    screen_class = build_screen(map_name, sample_class)
    if screen_class:
        sample.set_editor_property("screen_class", screen_class)

    # The camera the sample looks through: a 60 degree vertical field.
    if "camera" in scene:
        location, rotation = scene["camera"]
        camera = spawn(unreal.CameraActor, location, rotation)
        camera.camera_component.set_editor_property("field_of_view", 91.49)
        camera.camera_component.set_editor_property("constrain_aspect_ratio", False)
        camera.set_actor_label("Camera")
        sample.set_editor_property("view_camera", camera)

    # The characters, each a placed actor with its agent, voice, lip sync and animations.
    characters = []
    for index, (class_name, location, yaw, pick) in enumerate(scene.get("characters", [])):
        character_class = unreal.load_class(None, "/Script/Neocortex." + class_name)
        character = spawn(character_class, location, (0, 0, yaw))
        character.set_editor_property("pick_index", pick)
        character.set_actor_label("Cora" if index == 0 else "Character %d" % index)
        agent = character.get_editor_property("agent")
        if agent is not None:
            agent.set_editor_property("load_history_on_start", scene.get("history", False))
            if scene.get("text_only"):
                agent.set_editor_property("chat_lines_mode", unreal.ChatLinesMode.TEXT)
        characters.append(character)
    sample.set_editor_property("scene_characters", characters)

    # The shapes on the floor, each with its interactable.
    for shape, color_name, rgb, location in scene.get("shapes", []):
        actor = spawn(unreal.NeocortexSampleShape, location)
        actor.set_editor_property("shape", getattr(unreal.NeocortexSampleShapeKind, shape))
        actor.set_editor_property("color_name", color_name)
        actor.set_editor_property("color", unreal.LinearColor(*[pow(c / 255.0, 2.2) for c in rgb], 1.0))
        actor.set_actor_label("%s %s" % (color_name.capitalize(), shape.capitalize()))

    # Where the joining characters stand and wait.
    for prop, label, points in (("seats", "Seat", scene.get("seats", [])), ("offstage", "Offstage", scene.get("offstage", []))):
        actors = []
        for i, location in enumerate(points):
            point = spawn(unreal.TargetPoint, location)
            point.set_actor_label("%s %d" % (label, i + 1))
            actors.append(point)
        if actors:
            sample.set_editor_property(prop, actors)

    # The animations are loaded by name; listing them makes packaging include them.
    assets = [unreal.load_asset(CORA_DIR + "/cora")]
    assets += [unreal.load_asset("/Engine/BasicShapes/" + name) for name in ("Cube", "Cylinder", "BasicShapeMaterial")]
    assets += [unreal.load_asset(path.split(".")[0]) for path in unreal.EditorAssetLibrary.list_assets(CORA_DIR + "/Animations", recursive=False)]
    sample.set_editor_property("sample_assets", [asset for asset in assets if asset])


def build_maps():
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    for map_name, sample_class_name, has_floor in MAPS:
        path = MAPS_DIR + "/" + map_name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            unreal.EditorAssetLibrary.delete_asset(path)
        if not levels.new_level(path):
            unreal.log_error("[Neocortex samples] Could not create " + path)
            continue
        build_scene(map_name, sample_class_name, has_floor)
        levels.save_current_level()
        log("Built " + path)


import_checker()
make_checker_material()
import_cora()
build_maps()
log("Done.")
