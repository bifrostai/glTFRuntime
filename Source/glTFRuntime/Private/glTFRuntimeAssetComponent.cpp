#include "glTFRuntimeAssetComponent.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "glTFRuntimeFunctionLibrary.h"

UglTFRuntimeAssetComponent::UglTFRuntimeAssetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UglTFRuntimeAssetComponent::EditorLoadAsset()
{
	LoadAsset(GltfPath);
}

void UglTFRuntimeAssetComponent::LoadAsset(const FFilePath GltfFilePath)
{
	UE_LOG(LogGLTFRuntime, Display, TEXT("Loading asset %s"), *GltfFilePath.FilePath);
	UglTFRuntimeAsset* GltfRuntimeAsset = UglTFRuntimeFunctionLibrary::glTFLoadAssetFromFilename(GltfFilePath.FilePath, false, FglTFRuntimeConfig());
	if (!GltfRuntimeAsset)
	{
		UE_LOG(LogGLTFRuntime, Error, TEXT("Failed to load asset %s"), *GltfFilePath.FilePath);
		return;
	}
	LoadAsset(GltfRuntimeAsset);
	UE_LOG(LogGLTFRuntime, Display, TEXT("Successfully loaded asset"));

	GltfPath = GltfFilePath;
}

void UglTFRuntimeAssetComponent::LoadAsset(UglTFRuntimeAsset* Asset)
{
	for (FglTFRuntimeScene& Scene : Asset->GetScenes())
	{
		USceneComponent* SceneComponent = NewObject<USceneComponent>(this->GetOwner(), *FString::Printf(TEXT("Scene %d"), Scene.Index));
		SceneComponent->RegisterComponent();
		SceneComponent->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		AddInstanceComponent(SceneComponent);
		for (int32 NodeIndex : Scene.RootNodesIndices)
		{
			FglTFRuntimeNode Node;
			if (!Asset->GetNode(NodeIndex, Node))
			{
				return;
			}
			ProcessNode(Asset, SceneComponent, Node);
		}
	}
}

void UglTFRuntimeAssetComponent::ProcessNode(
	UglTFRuntimeAsset* Asset,
	USceneComponent* NodeParentComponent,
	FglTFRuntimeNode& Node,
	FglTFRuntimeStaticMeshConfig StaticMeshConfig /* = FglTFRuntimeStaticMeshConfig() */,
	FglTFRuntimeSkeletalMeshConfig SkeletalMeshConfig /* = FglTFRuntimeSkeletalMeshConfig() */)
{
	// skip bones/joints
	if (Asset->NodeIsBone(Node.Index))
	{
		return;
	}

	USceneComponent* NewComponent = nullptr;
	if (Node.CameraIndex != INDEX_NONE)
	{
		UCameraComponent* NewCameraComponent = NewObject<UCameraComponent>(this, GetSafeNodeName<UCameraComponent>(Node));
		NewCameraComponent->SetupAttachment(NodeParentComponent);
		NewCameraComponent->RegisterComponent();
		NewCameraComponent->SetRelativeTransform(Node.Transform);
		AddInstanceComponent(NewCameraComponent);
		Asset->LoadCamera(Node.CameraIndex, NewCameraComponent);
		NewComponent = NewCameraComponent;
	}
	else if (Node.MeshIndex < 0)
	{
		NewComponent = NewObject<USceneComponent>(this, GetSafeNodeName<USceneComponent>(Node));
		NewComponent->SetupAttachment(NodeParentComponent);
		NewComponent->RegisterComponent();
		NewComponent->SetRelativeTransform(Node.Transform);
		AddInstanceComponent(NewComponent);
	}
	else
	{
		if (Node.SkinIndex < 0)
		{
			UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, GetSafeNodeName<UStaticMeshComponent>(Node));
			StaticMeshComponent->SetupAttachment(NodeParentComponent);
			StaticMeshComponent->RegisterComponent();
			StaticMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(StaticMeshComponent);
			if (StaticMeshConfig.Outer == nullptr)
			{
				StaticMeshConfig.Outer = StaticMeshComponent;
			}
			UStaticMesh* StaticMesh = Asset->LoadStaticMesh(Node.MeshIndex, StaticMeshConfig);
			if (StaticMesh && !StaticMeshConfig.ExportOriginalPivotToSocket.IsEmpty())
			{
				UStaticMeshSocket* DeltaSocket = StaticMesh->FindSocket(FName(StaticMeshConfig.ExportOriginalPivotToSocket));
				if (DeltaSocket)
				{
					FTransform NewTransform = StaticMeshComponent->GetRelativeTransform();
					FVector DeltaLocation = -DeltaSocket->RelativeLocation * NewTransform.GetScale3D();
					DeltaLocation = NewTransform.GetRotation().RotateVector(DeltaLocation);
					NewTransform.AddToTranslation(DeltaLocation);
					StaticMeshComponent->SetRelativeTransform(NewTransform);
				}
			}
			StaticMeshComponent->SetStaticMesh(StaticMesh);
			// ReceiveOnStaticMeshComponentCreated(StaticMeshComponent, Node);
			NewComponent = StaticMeshComponent;
		}
		else
		{
			USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(this, GetSafeNodeName<USkeletalMeshComponent>(Node));
			SkeletalMeshComponent->SetupAttachment(NodeParentComponent);
			SkeletalMeshComponent->RegisterComponent();
			SkeletalMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(SkeletalMeshComponent);
			USkeletalMesh* SkeletalMesh = Asset->LoadSkeletalMesh(Node.MeshIndex, Node.SkinIndex, SkeletalMeshConfig);
			SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
			// ReceiveOnSkeletalMeshComponentCreated(SkeletalMeshComponent, Node);
			NewComponent = SkeletalMeshComponent;
		}
	}

	if (!NewComponent)
	{
		return;
	}

	// check for audio emitters
	for (const int32 EmitterIndex : Node.EmitterIndices)
	{
		FglTFRuntimeAudioEmitter AudioEmitter;
		if (Asset->LoadAudioEmitter(EmitterIndex, AudioEmitter))
		{
			UAudioComponent* AudioComponent = NewObject<UAudioComponent>(this, *AudioEmitter.Name);
			AudioComponent->SetupAttachment(NewComponent);
			AudioComponent->RegisterComponent();
			AudioComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(AudioComponent);
			Asset->LoadEmitterIntoAudioComponent(AudioEmitter, AudioComponent);
			AudioComponent->Play();
		}
	}

	// check for animations
	if (!NewComponent->IsA<USkeletalMeshComponent>())
	{
		TArray<UglTFRuntimeAnimationCurve*> ComponentAnimationCurves = Asset->LoadAllNodeAnimationCurves(Node.Index);
		TMap<FString, UglTFRuntimeAnimationCurve*> ComponentAnimationCurvesMap;
		for (UglTFRuntimeAnimationCurve* ComponentAnimationCurve : ComponentAnimationCurves)
		{
			if (!CurveBasedAnimations.Contains(NewComponent))
			{
				CurveBasedAnimations.Add(NewComponent, ComponentAnimationCurve);
				CurveBasedAnimationsTimeTracker.Add(NewComponent, 0);
			}
			DiscoveredCurveAnimationsNames.Add(ComponentAnimationCurve->glTFCurveAnimationName);
			ComponentAnimationCurvesMap.Add(ComponentAnimationCurve->glTFCurveAnimationName, ComponentAnimationCurve);
		}
		DiscoveredCurveAnimations.Add(NewComponent, ComponentAnimationCurvesMap);
	}
	else
	{
		USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewComponent);
		FglTFRuntimeSkeletalAnimationConfig SkeletalAnimationConfig;
		UAnimSequence* SkeletalAnimation = Asset->LoadNodeSkeletalAnimation(SkeletalMeshComponent->SkeletalMesh, Node.Index, SkeletalAnimationConfig);
		if (SkeletalAnimation)
		{
			SkeletalMeshComponent->AnimationData.AnimToPlay = SkeletalAnimation;
			SkeletalMeshComponent->AnimationData.bSavedLooping = true;
			SkeletalMeshComponent->AnimationData.bSavedPlaying = true;
			SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		}
	}

	for (int32 ChildIndex : Node.ChildrenIndices)
	{
		FglTFRuntimeNode Child;
		if (!Asset->GetNode(ChildIndex, Child))
		{
			return;
		}
		ProcessNode(Asset, NewComponent, Child);
	}
}

void UglTFRuntimeAssetComponent::AddInstanceComponent(USceneComponent* SceneComponent)
{
	AActor* OwnerActor = this->GetOwner();
	if (OwnerActor)
	{
		OwnerActor->AddInstanceComponent(SceneComponent);
	}
}

void UglTFRuntimeAssetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (TPair<USceneComponent*, UglTFRuntimeAnimationCurve*>& Pair : CurveBasedAnimations)
	{
		// the curve could be null
		if (!Pair.Value)
		{
			continue;
		}
		float MinTime;
		float MaxTime;
		Pair.Value->GetTimeRange(MinTime, MaxTime);

		float CurrentTime = CurveBasedAnimationsTimeTracker[Pair.Key];
		if (CurrentTime > Pair.Value->glTFCurveAnimationDuration)
		{
			CurveBasedAnimationsTimeTracker[Pair.Key] = 0;
			CurrentTime = 0;
		}

		if (CurrentTime >= MinTime)
		{
			FTransform FrameTransform = Pair.Value->GetTransformValue(CurveBasedAnimationsTimeTracker[Pair.Key]);
			Pair.Key->SetRelativeTransform(FrameTransform);
		}
		CurveBasedAnimationsTimeTracker[Pair.Key] += DeltaTime;
	}
}
