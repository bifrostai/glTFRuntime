#pragma once

#include "CoreMinimal.h"
#include "glTFRuntimeAsset.h"

#include "glTFRuntimeAssetComponent.generated.h"

UCLASS(ClassGroup = (glTFRuntime), HideCategories = (Tags, Activation, Cooking, AssetUserData, Collision, Rendering, Physics, LOD))
class GLTFRUNTIME_API UglTFRuntimeAssetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UglTFRuntimeAssetComponent();

	void LoadAsset(const FFilePath GltfFilePath);
	void LoadAsset(UglTFRuntimeAsset* Asset);

protected:
	// TODO - test animation in TickComponent
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void ProcessNode(
		UglTFRuntimeAsset* Asset,
		USceneComponent* NodeParentComponent,
		FglTFRuntimeNode& Node,
		FglTFRuntimeStaticMeshConfig StaticMeshConfig = FglTFRuntimeStaticMeshConfig(),
		FglTFRuntimeSkeletalMeshConfig SkeletalMeshConfig = FglTFRuntimeSkeletalMeshConfig());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GltfRuntime")
	TMap<USceneComponent*, UglTFRuntimeAnimationCurve*> CurveBasedAnimations;

	TMap<USceneComponent*, float> CurveBasedAnimationsTimeTracker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GltfRuntime")
	TSet<FString> DiscoveredCurveAnimationsNames;

	TMap<USceneComponent*, TMap<FString, UglTFRuntimeAnimationCurve*>> DiscoveredCurveAnimations;

	template <typename T>
	FName GetSafeNodeName(const FglTFRuntimeNode& Node)
	{
		return MakeUniqueObjectName(this, T::StaticClass(), *Node.Name);
	}

private:
	void AddInstanceComponent(USceneComponent* SceneComponent);

	UPROPERTY(EditAnywhere, Category = "Editor Debugging")
	FFilePath GltfPath;

	UFUNCTION(CallInEditor, Category = "Editor Debugging")
	void EditorLoadAsset();
};
