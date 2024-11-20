// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Jet.generated.h"


UCLASS()
class UNREALSIM_API AJet : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AJet();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//Mesh components
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* body;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* Robot;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_FR;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_FL;
	UPROPERTY(VisibleAnywhere, Category = "Mesh Components")
	UStaticMeshComponent* wheel_BM;

	//Camera Control Components
	UPROPERTY(VisibleAnywhere, Category = "Camera Components")
	class UCameraComponent* backCamera;

	//Input components
	UPROPERTY(EditAnywhere, Category = "EnhancedInput") //TODO There's no way I can't manually pass these in through the header without blueprint. Figure that out adn save me a step
	class UInputMappingContext* IM_main; //Necessary to define IM_Main in .cpp for input. This also means you have to define in blueprint tho
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_stick;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_throttle;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_rudders;
	UPROPERTY(EditAnywhere, Category = "EnhancedInput")
	class UInputAction* IA_landingGear;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	void stickControl(const FInputActionInstance& Instance);
	void throttleControl(const FInputActionInstance& Instance);
	void ruddersControl(const FInputActionInstance& Instance); //Note this one is plural and the others aren't, future me
	void landingGearControl(const FInputActionInstance& Instance);
};
