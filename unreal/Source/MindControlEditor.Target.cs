using UnrealBuildTool;
using System.Collections.Generic;
public class MindControlEditorTarget : TargetRules
{
    public MindControlEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("MindControl");
    }
}
