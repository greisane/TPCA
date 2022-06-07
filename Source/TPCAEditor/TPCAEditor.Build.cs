// This source code is licensed under the MIT license found in the LICENSE file in the root directory of this source tree.

using UnrealBuildTool;

public class TPCAEditor : ModuleRules
{
	public TPCAEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = true;

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"TPCAEditor/Private"
			});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"UnrealEd",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"TPCA",
			});
	}
}
