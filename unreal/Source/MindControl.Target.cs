using UnrealBuildTool;
using System.Collections.Generic;
public class MindControlTarget : TargetRules
{
    public MindControlTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        ExtraModuleNames.Add("MindControl");
    }
}
