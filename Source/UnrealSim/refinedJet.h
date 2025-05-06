// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Components/PrimitiveComponent.h" // get angular and linear damping
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h" // For UStaticMeshComponent
#include "GameFramework/PlayerController.h" // For accessing player controller
#include "refinedJet.generated.h"

UCLASS()
class UNREALSIM_API ArefinedJet : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ArefinedJet();

	//Mesh components
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Mesh Components")
	UStaticMeshComponent* body;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_FR;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_FL;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_BM;

	//Camera Control Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Components")
	class UCameraComponent* backCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Capture")
	USceneCaptureComponent2D* GPSCam;

	//Input components
	UPROPERTY(EditAnywhere, Category = "EnhancedInput") //TODO There's no way I can't manually pass these in through the header without blueprint. Figure that out adn save me a step
	class UInputMappingContext* IM_main; //Necessary to define IM_Main in .cpp for input. This also means you have to define in blueprint tho
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_stick;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_throttle;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_rudder;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_landingGear;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_toeBrake;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_spoiler;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void stickControl(const FInputActionInstance& Instance);
	void stickRelease();
	void throttleControl(const FInputActionInstance& Instance);
	void rudderControl(const FInputActionInstance& Instance);
	void landingGearControl(const FInputActionInstance& Instance);
	void toeBrakeControl(const FInputActionInstance& Instance);
	void spoilerControl(const FInputActionInstance& Instance);
};
