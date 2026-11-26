import unreal
import os
root=os.environ.get("MEDIEVAL_ASSET_DIR")
if not root or not os.path.isdir(root):
    raise RuntimeError("MEDIEVAL_ASSET_DIR must point to generated GLB assets")
dest="/Game/Environment/MindControl/Medieval"
names=["H5_Cottage","A2_ArmorStand","A2_Shelves","A2_Armory","A2_Shield","Blacksmith_Sword","Blacksmith_Anvil","Sorcerer_Potion","Blacksmith_Shop","Sorcerer_Shop"]+["Herb_"+n for n in ("Lavender","Chamomile","Bloodleaf","Mint","Silverleaf","BlueSage","Firebloom","Moonflower","LemonBalm","Yarrow")]
for name in names:
    source=os.path.join(root,name+".glb")
    if not os.path.isfile(source):
        raise RuntimeError("Missing "+source)
    manager=unreal.InterchangeManager.get_interchange_manager_scripted()
    data=unreal.InterchangeManager.create_source_data(source)
    params=unreal.ImportAssetParameters()
    params.is_automated=True
    target=dest+"/"+name
    if not manager.import_asset(target,data,params):
        raise RuntimeError("Interchange import failed: "+name)
    meshes=[]
    for asset_path in unreal.EditorAssetLibrary.list_assets(target,recursive=True):
        asset=unreal.EditorAssetLibrary.load_asset(asset_path)
        if isinstance(asset,unreal.StaticMesh):
            meshes.append(asset)
    if not meshes:
        raise RuntimeError("No static meshes imported: "+name)
    for asset in meshes:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    mesh=meshes[0]
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    unreal.log("MEDIEVAL_IMPORTED "+name)
unreal.log("MEDIEVAL_IMPORT_COMPLETE "+str(len(names)))
