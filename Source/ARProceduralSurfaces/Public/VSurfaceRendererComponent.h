// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VSurfaceRendererComponent.generated.h"

class UARPlaneGeometry;
class UProceduralMeshComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ARPROCEDURALSURFACES_API UVSurfaceRendererComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UVSurfaceRendererComponent();

private:
	UPROPERTY()
	TMap<UARPlaneGeometry*, TWeakObjectPtr<UProceduralMeshComponent>> ProceduralSurfaceMap;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	float EdgeFeatheringDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	UMaterial* SurfaceMaterial;
	
protected:
	virtual void BeginPlay() override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ClearUnusedSurfaces();
	void UpdateSurface(UARPlaneGeometry* Surface, UProceduralMeshComponent* ProceduralMeshComponent);
		
};
