// Fill out your copyright notice in the Description page of Project Settings.


#include "Jet.h"
#include <cmath> //contains interpolation function
#include "Components/StaticMeshComponent.h" //Create default subobject
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h" //Set up file paths for loading in meshes
#include "InputMappingContext.h" //Setups for enhanced input
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" //Posession for player


const float ROTSPEED = 10.0f; //blanket rotation speed. Will eventually be decided via joystick input
float LandingGearY = -50.f;
int moveSpeed = 50; //Movement speed
//float gravity = -1;

//Tracked speed variables for interpolation of movement
float pitch = 0; //up and down
float yaw = 0; //left and right
float roll = 0; //side to side


// Sets default values
AJet::AJet()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//src component setup: https://forums.unrealengine.com/t/add-component-to-actor-in-c-the-final-word/646838
	//src cam setup: https://www.youtube.com/watch?app=desktop&v=qJDIABF-IYs

	//Create components
	body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Robot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Robot"));
	backCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	wheel_FR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_FR"));
	wheel_FL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_FL"));
	wheel_BM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_BM"));

	//Define path to meshes
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_body(TEXT("StaticMesh'/Game/Mesh/ShipBody.ShipBody'"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_Robot(TEXT("StaticMesh'/Game/Mesh/Robot.Robot'"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_wheel(TEXT("StaticMesh'/Game/Mesh/LandingGear.LandingGear'"));

	//Link components to meshes
	if (MeshAsset_body.Succeeded() and MeshAsset_wheel.Succeeded() and MeshAsset_Robot.Succeeded())
	{
		body->SetStaticMesh(MeshAsset_body.Object);
		Robot->SetStaticMesh(MeshAsset_Robot.Object);
		wheel_FR->SetStaticMesh(MeshAsset_wheel.Object);
		wheel_FL->SetStaticMesh(MeshAsset_wheel.Object);
		wheel_BM->SetStaticMesh(MeshAsset_wheel.Object);

		//define hierarchy
		backCamera->SetupAttachment(body);
		Robot->SetupAttachment(body);
		wheel_FR->SetupAttachment(body);
		wheel_FL->SetupAttachment(wheel_FR);
		wheel_BM->SetupAttachment(wheel_FR);

		//Define Starting location relative to body (landing gear down)
		backCamera->SetRelativeLocation(FVector(-305, 0, 30));
		Robot->SetRelativeLocation(FVector(0, 0, 40));
		wheel_FR->SetRelativeLocation(FVector(25, 15, -50)); //TODO: Define one fvector above and modify its values for the other two later
		wheel_FL->SetRelativeLocation(FVector(0, -30, 0));
		wheel_BM->SetRelativeLocation(FVector(-75, -15, 0));
	}
}
// Called when the game starts or when spawned
void AJet::BeginPlay()
{
	Super::BeginPlay();
	//int actorXLoc = GetActorLocation().X;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	if (PlayerController)
	{
		//Possess this actor
		PlayerController->Possess(this);
	}
	
}

// Called every frame
void AJet::Tick(float DeltaTime)
{
	// Get the forward vector of the actor
	FVector ForwardVector = GetActorForwardVector();

	// Calculate the new location based on forward movement
	FVector NewLocation = GetActorLocation() + (ForwardVector * moveSpeed * DeltaTime);

	// Set the new location
	SetActorLocation(NewLocation);

}

// Called to bind functionality to input
void AJet::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//src: https://www.youtube.com/watch?v=O-3PNiTHlE0
	// Add input mapping context
	if (APlayerController * PlayerController = Cast<APlayerController>(Controller)) {
		// Get local player subsystem
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())) {
			// Add input context
			Subsystem->AddMappingContext(IM_main, 0);
		}
	}

	//Call Enhanced input value as Var input, cast into callback function for user input if valid
	if (UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		Input->BindAction(IA_stick, ETriggerEvent::Triggered, this, &AJet::stickControl);
		Input->BindAction(IA_throttle, ETriggerEvent::Triggered, this, &AJet::throttleControl);
		Input->BindAction(IA_rudders, ETriggerEvent::Triggered, this, &AJet::ruddersControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Triggered, this, &AJet::landingGearControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Triggered, this, &AJet::landingGearControl);
	}
}


// Get stick control values and rotate jet appropriately
void AJet::stickControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //Controler input from a stick. Gets an x and y value to represent pitch and roll

	UE_LOG(LogTemp, Warning, TEXT("Rotation is: %f,%f"), AxisValue.X, AxisValue.Y); // TODO: Create a widget to display this instead. TODO. Why is x y and y x?

	//lerp(starting movement, speed, ending value?)
	float lerpout = lerp(AxisValue.X, 1, pitch);
	printf("Lerp  = %f", lerpout);
	FRotator newRot = FRotator(ROTSPEED * pitch, 0.0f, ROTSPEED * AxisValue.Y);

	AddActorLocalRotation(newRot);
}

void AJet::throttleControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //A-10C jets are dual-engine. X=left engine, Y=Right engine
	UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), AxisValue.X, AxisValue.Y);

	moveSpeed = (AxisValue.X * 1000 + AxisValue.Y * 1000);

	//Allows use of dual engine setup to turn jet on Yaw axis
	float rotationSpeed = (AxisValue.Y - AxisValue.X); //Y rotation is negative so we want to go negative if X is bigger

	//This will probably be disabled temporarily while I do other stuff later. Remember to uncomment it when working on the real thing to make sure dead zones are set and good
	FRotator newRot = FRotator(0.0f, ROTSPEED * rotationSpeed, 0.0f); //TODO: Call ruddersControl and pass float value in instead. RuddersControl takes a different value type. Make a new function that they BOTH call that takes a float made from the new type
	AddActorLocalRotation(newRot);
}

void AJet::ruddersControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>(); //Rudders will go between positive and negative to decide rotation so we don't need a vector
	UE_LOG(LogTemp, Warning, TEXT("Yaw is: %f"), FloatValue);
	FRotator newRot = FRotator(0.0f, ROTSPEED * FloatValue, 0.0f);
	AddActorLocalRotation(newRot);
}

void AJet::landingGearControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	UE_LOG(LogTemp, Warning, TEXT("Gear is: %f"), FloatValue);

	if (FloatValue == 1 and wheel_FR->GetRelativeLocation.Z) {
		wheel_FR->SetRelativeLocation(FVector(25, 15, 0.f));
	}
	else {
		wheel_FR->SetRelativeLocation(FVector(25, 15, -50));
	}
}

//TODO: Add button for landing gear






//Thruster stick = throttle

//TODO: Map a yaw control function that can work with rudder pedals AND for the dual control thruster setup

//TODO: If thrust gets low enough then gravity should start sinking our jet
//TODO: IA should probably be imported directly into file instead of doing it via blueprints