import unreal
import os
root=os.environ.get("MEDIEVAL_ASSET_DIR")
if not root or not os.path.isdir(root):
    raise RuntimeError("MEDIEVAL_ASSET_DIR must point to generated FBX assets")
dest="/Game/Environment/MindControl/Medieval"
names=["H5_Cottage","A2_ArmorStand","A2_Shelves","A2_Armory","A2_Shield","Blacksmith_Sword","Blacksmith_Anvil","Sorcerer_Potion","Blacksmith_Shop","Sorcerer_Shop"]+["Herb_"+n for n in ("Lavender","Chamomile","Bloodleaf","Mint","Silverleaf","BlueSage","Firebloom","Moonflower","LemonBalm","Yarrow")]
for name in names:
    source=os.path.join(root,name+".fbx")
    if not os.path.isfile(source):
        raise RuntimeError("Missing "+source)
    task=unreal.AssetImportTask()
    task.filename=source
    task.destination_path=dest
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    options=unreal.FbxImportUI()
    options.import_mesh=True
    options.import_as_skeletal=False
    options.import_materials=True
    options.import_textures=True
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    task.options=options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.EditorAssetLibrary.load_asset(dest+"/"+name)
    if not isinstance(mesh,unreal.StaticMesh):
        raise RuntimeError("Static mesh import failed: "+name)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log("MEDIEVAL_IMPORTED "+name)
unreal.log("MEDIEVAL_IMPORT_COMPLETE "+str(len(names)))
