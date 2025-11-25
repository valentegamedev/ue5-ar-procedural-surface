// Fill out your copyright notice in the Description page of Project Settings.


#include "VSurfaceRendererComponent.h"

#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"

UVSurfaceRendererComponent::UVSurfaceRendererComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVSurfaceRendererComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UVSurfaceRendererComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	//Get all geometries from AR session
	TArray<UARTrackedGeometry*> AllGeometries = UARBlueprintLibrary::GetAllGeometries();
	for (UARTrackedGeometry* Geometry : AllGeometries)
	{
		//Check if the geometry is a plane
		if (UARPlaneGeometry* PlaneGeometry = Cast<UARPlaneGeometry>(Geometry))
		{
			// Try to find existing mesh component based on the plane geometry (surface)
			TWeakObjectPtr<UProceduralMeshComponent> CurrentProceduralMeshComponentRef = ProceduralSurfaceMap.FindRef(PlaneGeometry);
			
			// If not found or invalid, create a new one
			if (!CurrentProceduralMeshComponentRef.IsValid())
			{
				AActor* Owner = GetOwner();
				if (Owner)
				{
					UActorComponent* ActorComponentRef = Owner->AddComponentByClass(
						UProceduralMeshComponent::StaticClass(),
						true,
						FTransform::Identity,
						false);
					if (UProceduralMeshComponent* ProceduralMeshComponentRef = Cast<UProceduralMeshComponent>(ActorComponentRef))
					{
						//Assign material to the procedural mesh
						if (SurfaceMaterial)
						{
							ProceduralMeshComponentRef->SetMaterial(0, SurfaceMaterial);
						}
						//Add to map
						ProceduralSurfaceMap.Add(PlaneGeometry, ProceduralMeshComponentRef);
						//Set current procedural mesh component reference to the newly created one
						CurrentProceduralMeshComponentRef = ProceduralMeshComponentRef;
					}
				}
			}
			
			//Update the procedural mesh component for this plane (surface)
			if (CurrentProceduralMeshComponentRef.IsValid())
			{
				UpdateSurface(PlaneGeometry, CurrentProceduralMeshComponentRef.Get());
			}
		}
	}
	
	//Clear any unused surfaces
	ClearUnusedSurfaces();
	
	//Print the number of surfaces found so far
	FName DebugKey(TEXT("Debug"));
	const FString Msg = FString::Printf(TEXT("Surfaces Found: %d"), ProceduralSurfaceMap.Num());
	UKismetSystemLibrary::PrintString(this, Msg, true, true, FLinearColor::Green, 1.0f, DebugKey);
}

void UVSurfaceRendererComponent::ClearUnusedSurfaces()
{
	//Loop through the map and find any planes (surfaces) that are no longer being tracked
	TArray<UARPlaneGeometry*> KeysToRemove;
	
	for (const auto& Pair : ProceduralSurfaceMap)
	{
		UARPlaneGeometry* PlaneGeometry = Pair.Key;
		
		// Check if the plane is invalid OR no longer tracking
		if (PlaneGeometry->GetTrackingState() == EARTrackingState::StoppedTracking || 
			PlaneGeometry->GetTrackingState() == EARTrackingState::NotTracking)
		{
			KeysToRemove.Add(PlaneGeometry);
		}
	}
	
	//Remove the unused planes (surfaces) from the map and destroy their procedural mesh components
	for (UARPlaneGeometry* Key : KeysToRemove)
	{
		TWeakObjectPtr<UProceduralMeshComponent> ProceduralMeshComponent = ProceduralSurfaceMap.FindRef(Key);
		if (ProceduralMeshComponent.IsValid())
		{
			ProceduralMeshComponent->DestroyComponent();
		}
		ProceduralSurfaceMap.Remove(Key);
	}
}

void UVSurfaceRendererComponent::UpdateSurface(UARPlaneGeometry* Surface, UProceduralMeshComponent* ProceduralMeshComponent)
{
	//STEP 1: Get the Raw Shape
	
	// Get the boundary vertices of the surface
	TArray<FVector> BoundaryVertices;
	BoundaryVertices = Surface->GetBoundaryPolygonInLocalSpace();
	int BoundaryVerticesNum = BoundaryVertices.Num();

	// If there are less than 3 boundary vertices, clear the mesh section and return
	if (BoundaryVerticesNum < 3)
	{
		ProceduralMeshComponent->ClearMeshSection(0);
		return;
	}

	// Each boundary vertex will have an interior vertex for feathering, so total vertices is double
	int TotalVerticesNum = BoundaryVerticesNum * 2;

	// Prepare arrays for surface data
	TArray<FVector> Vertices;
	TArray<FLinearColor> VertexColors;
	TArray<int> Indices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	//STEP 2: Create a "Double Border"
	
	// Generate vertices, normals, uvs, and vertex colors
	FVector PlaneNormal = Surface->GetLocalToWorldTransform().GetRotation().GetUpVector();
	for (int i = 0; i < BoundaryVerticesNum; i++)
	{
		FVector BoundaryPoint = BoundaryVertices[i];
		
		// Calculate interior point for feathering
		float BoundaryToCenterDist = BoundaryPoint.Size();
		float FeatheringDist = FMath::Min(BoundaryToCenterDist, EdgeFeatheringDistance);
		FVector InteriorPoint = BoundaryPoint - BoundaryPoint.GetUnsafeNormal() * FeatheringDist;

		Vertices.Add(BoundaryPoint);
		Vertices.Add(InteriorPoint);

		// UVs can be based on local XY coordinates
		UVs.Add(FVector2D(BoundaryPoint.X, BoundaryPoint.Y));
		UVs.Add(FVector2D(InteriorPoint.X, InteriorPoint.Y));

		// Normals: both vertices have the same normal
		Normals.Add(PlaneNormal);
		Normals.Add(PlaneNormal);

		// Vertex colors: boundary vertex fully transparent, interior vertex opaque
		VertexColors.Add(FLinearColor(0.0f, 0.f, 0.f, 0.f));
		VertexColors.Add(FLinearColor(0.0f, 0.f, 0.f, 1.f));
	}

	//STEP 3: "Connect the Dots" with Triangles

	// Perimeter triangles
	for (int i = 0; i < BoundaryVerticesNum - 1; i++)
	{
		Indices.Add(i * 2);
		Indices.Add(i * 2 + 2);
		Indices.Add(i * 2 + 1);

		Indices.Add(i * 2 + 1);
		Indices.Add(i * 2 + 2);
		Indices.Add(i * 2 + 3);
	}

	// First triangle of the last perimeter segment
	Indices.Add((BoundaryVerticesNum - 1) * 2);
	Indices.Add(0);
	Indices.Add((BoundaryVerticesNum - 1) * 2 + 1);

	// Second triangle of the last perimeter segment
	Indices.Add((BoundaryVerticesNum - 1) * 2 + 1);
	Indices.Add(0);
	Indices.Add(1);
	
	//STEP 4: Fill the Center
	
	// interior triangles
	for (int i = 3; i < TotalVerticesNum - 1; i += 2)
	{
		Indices.Add(1);
		Indices.Add(i);
		Indices.Add(i + 2);
	}

	// Create or update the mesh section
	ProceduralMeshComponent->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UVs, VertexColors, TArray<FProcMeshTangent>(), false);

	//STEP 5: Positioning
	
	// Set the component transform to plane's transform.
	ProceduralMeshComponent->SetWorldTransform(Surface->GetLocalToWorldTransform());
}

