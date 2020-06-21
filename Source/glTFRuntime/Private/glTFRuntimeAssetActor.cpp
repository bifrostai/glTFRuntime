// Copyright 2020 Roberto De Ioris


#include "glTFRuntimeAssetActor.h"

// Sets default values
AglTFRuntimeAssetActor::AglTFRuntimeAssetActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	AssetRoot = CreateDefaultSubobject<USceneComponent>(TEXT("AssetRoot"));
	RootComponent = AssetRoot;
}

// Called when the game starts or when spawned
void AglTFRuntimeAssetActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Asset)
	{
		return;
	}

	TArray<FglTFRuntimeScene> Scenes = Asset->GetScenes();
	for (FglTFRuntimeScene& Scene : Scenes)
	{
		USceneComponent* SceneComponent = NewObject<USceneComponent>(this, *FString::Printf(TEXT("Scene %d"), Scene.Index));
		SceneComponent->SetupAttachment(RootComponent);
		SceneComponent->RegisterComponent();
		AddInstanceComponent(SceneComponent);
		for (int32 NodeIndex : Scene.RootNodesIndices)
		{
			FglTFRuntimeNode Node;
			if (!Asset->GetNode(NodeIndex, Node))
			{
				return;
			}
			ProcessNode(SceneComponent, Node);
		}
	}
}

void AglTFRuntimeAssetActor::ProcessNode(USceneComponent* NodeParentComponent, FglTFRuntimeNode& Node)
{
	USceneComponent* NewComponent = nullptr;
	if (Node.MeshIndex < 0)
	{
		NewComponent = NewObject<USceneComponent>(this, *Node.Name);
		NewComponent->SetupAttachment(NodeParentComponent);
		NewComponent->RegisterComponent();
		NewComponent->SetRelativeTransform(Node.Transform);
		AddInstanceComponent(NewComponent);
	}
	else
	{
		if (Node.SkinIndex < 0)
		{
			UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(this, *Node.Name);
			StaticMeshComponent->SetupAttachment(NodeParentComponent);
			StaticMeshComponent->RegisterComponent();
			StaticMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(StaticMeshComponent);
			FglTFRuntimeStaticMeshConfig StaticMeshConfig;
			StaticMeshComponent->SetStaticMesh(Asset->LoadStaticMesh(Node.MeshIndex, StaticMeshConfig));
			NewComponent = StaticMeshComponent;
		}
		else
		{
			USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(this, *Node.Name);
			SkeletalMeshComponent->SetupAttachment(NodeParentComponent);
			SkeletalMeshComponent->RegisterComponent();
			SkeletalMeshComponent->SetRelativeTransform(Node.Transform);
			AddInstanceComponent(SkeletalMeshComponent);
			FglTFRuntimeSkeletalMeshConfig SkeletalMeshConfig;
			SkeletalMeshComponent->SetSkeletalMesh(Asset->LoadSkeletalMesh(Node.MeshIndex, Node.SkinIndex, SkeletalMeshConfig));
			NewComponent = SkeletalMeshComponent;
		}
	}

	if (!NewComponent)
	{
		return;
	}

	// check for animations
	if (!NewComponent->IsA<USkeletalMeshComponent>())
	{
		UglTFRuntimeAnimationCurve* AnimationCurve = Asset->LoadNodeAnimationCurve(Node.Index);
		if (AnimationCurve)
		{
			CurveBasedAnimations.Add(NewComponent, AnimationCurve);
		}
	}

	for (int32 ChildIndex : Node.ChildrenIndices)
	{
		FglTFRuntimeNode Child;
		if (!Asset->GetNode(ChildIndex, Child))
		{
			return;
		}
		ProcessNode(NewComponent, Child);
	}
}

// Called every frame
void AglTFRuntimeAssetActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
