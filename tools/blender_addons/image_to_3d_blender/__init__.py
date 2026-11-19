bl_info = {"name": "Image to 3D Studio", "author": "OpenAI", "version": (0, 1, 1), "blender": (3, 6, 0), "location": "View3D > Sidebar > Image 3D", "description": "Import GLB/FBX/OBJ models and load image references", "category": "Import-Export"}
import bpy
from bpy.props import StringProperty
from bpy.types import Operator, Panel, PropertyGroup

class Image3DSettings(PropertyGroup):
    image_path: StringProperty(name="Reference image", subtype="FILE_PATH")
    model_path: StringProperty(name="3D model", subtype="FILE_PATH")

class IMAGE3D_OT_reference(Operator):
    bl_idname = "image3d.reference"
    bl_label = "Add reference image"
    def execute(self, context):
        p = bpy.path.abspath(context.scene.image3d_settings.image_path)
        if not p:
            self.report({'ERROR'}, "Choose a reference image")
            return {'CANCELLED'}
        try:
            img = bpy.data.images.load(p, check_existing=True)
            bpy.ops.object.empty_add(type='IMAGE')
            context.object.data = img
            context.object.name = "Image 3D Reference"
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}

class IMAGE3D_OT_import(Operator):
    bl_idname = "image3d.import_model"
    bl_label = "Import generated 3D model"
    def execute(self, context):
        p = bpy.path.abspath(context.scene.image3d_settings.model_path)
        ext = p.lower().rsplit(".", 1)[-1]
        try:
            if ext in ('glb', 'gltf'):
                bpy.ops.import_scene.gltf(filepath=p)
            elif ext == 'fbx':
                bpy.ops.import_scene.fbx(filepath=p)
            elif ext == 'obj':
                if hasattr(bpy.ops.wm, 'obj_import'):
                    bpy.ops.wm.obj_import(filepath=p)
                else:
                    bpy.ops.import_scene.obj(filepath=p)
            else:
                self.report({'ERROR'}, "Choose a GLB, GLTF, FBX or OBJ")
                return {'CANCELLED'}
            return {'FINISHED'}
        except Exception as exc:
            self.report({'ERROR'}, str(exc))
            return {'CANCELLED'}

class IMAGE3D_PT_main(Panel):
    bl_label = "Image to 3D Studio"
    bl_idname = "IMAGE3D_PT_main"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = "Image 3D"
    def draw(self, context):
        layout = self.layout
        settings = context.scene.image3d_settings
        layout.prop(settings, "image_path")
        layout.operator("image3d.reference")
        layout.separator()
        layout.prop(settings, "model_path")
        layout.operator("image3d.import_model")
        layout.label(text="AI generation requires external model")

classes = (Image3DSettings, IMAGE3D_OT_reference, IMAGE3D_OT_import, IMAGE3D_PT_main)
def register():
    for cls in classes: bpy.utils.register_class(cls)
    bpy.types.Scene.image3d_settings = bpy.props.PointerProperty(type=Image3DSettings)
def unregister():
    del bpy.types.Scene.image3d_settings
    for cls in reversed(classes): bpy.utils.unregister_class(cls)
if __name__ == "__main__": register()
