// Fill out your copyright notice in the Description page of Project Settings.


#include "refinedJet.h"
#include <cmath> //contains interpolation function
#include "Components/StaticMeshComponent.h" //Create default subobject
#include "Components/PrimitiveComponent.h" // for torque
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h" //Set up file paths for loading in meshes
#include "InputMappingContext.h" //Setups for enhanced input
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" //Posession for player
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

//float turnAmount;// TODO: these are constant multipliers. I should find good vals
float throttleAmount = 800000; // Must be obscenely high to see any affect on the jet
float turnAmount = 10;
float liftAmount = 4000;

//float throttle, pitch, yaw, roll;
FVector torque; //Pitch, roll, yaw (in that order) movement on plane // TODO: Multiply by a float that's between 0 and 1 and set by force so a still plane can't steer
FVector force; // forward force on Jet plane (from throttle)

// Sets default values
ArefinedJet::ArefinedJet()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//src component setup: https://forums.unrealengine.com/t/add-component-to-actor-in-c-the-final-word/646838
	//src cam setup: https://www.youtube.com/watch?app=desktop&v=qJDIABF-IYs

	//Create components
	body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	backCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	wheel_FR = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_FR"));
	wheel_FL = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_FL"));
	wheel_BM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wheel_BM"));

	//Define path to meshes
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_body(TEXT("StaticMesh'/Game/Mesh/HornetStaticMesh.HornetStaticMesh'"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_wheel(TEXT("StaticMesh'/Game/Mesh/LandingGear.LandingGear'"));

	//Link components to meshes
	if (MeshAsset_body.Succeeded() and MeshAsset_wheel.Succeeded())
	{
		body->SetStaticMesh(MeshAsset_body.Object);
		wheel_FR->SetStaticMesh(MeshAsset_wheel.Object);
		wheel_FL->SetStaticMesh(MeshAsset_wheel.Object);
		wheel_BM->SetStaticMesh(MeshAsset_wheel.Object);
		GPSCam = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));

		//define hierarchy
		backCamera->SetupAttachment(body);
		GPSCam->SetupAttachment(body);
		wheel_FR->SetupAttachment(body);
		wheel_FL->SetupAttachment(wheel_FR);
		wheel_BM->SetupAttachment(wheel_FR);

		//Define Starting location relative to body for all components (landing gear starts down)
		backCamera->SetRelativeLocation(FVector(-305, 0, 30));
		wheel_FR->SetRelativeLocation(FVector(25, 15, -50));
		wheel_FL->SetRelativeLocation(FVector(0, -30, 0));
		wheel_BM->SetRelativeLocation(FVector(-75, -15, 0));
	}
}

// Called when the game starts or when spawned
void ArefinedJet::BeginPlay()
{
	Super::BeginPlay();

	// Prevents bug where numbers are obscenely high on startup
	torque = FVector(0, 0, 0);
	force = FVector(0, 0, 0);

	// Defaults I built physics around
	body->SetLinearDamping(5);
	body->SetAngularDamping(10);
}

// TODO: Lerp values towards 0 once they're not being used anymore. Or find some constant value that they have to fight against to represent wind
// TODO: normalize torque so rotation feels better. rotating and trying to turn does NOT work. Vals must be normalized
// TODO: Add brakes

// Called every frame
void ArefinedJet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Apply forces to jet. Forces calculated inside of user input functions below
	body->AddTorqueInDegrees(torque, NAME_None, true);
	body->AddForce(GetActorForwardVector() * force * 800000); // TODO: Use quats to noramlize torque to make airplane easier to fly
	UE_LOG(LogTemp, Warning, TEXT("Force: %f, %f, %f"), force.X, force.Y, force.Z);

	//Calculate Lift
	FVector forward = GetOwner()->GetActorForwardVector();
	FVector velocity = body->GetPhysicsLinearVelocity();
	float localForwardSpeed = FVector::DotProduct(velocity, forward);

	float liftForceMagnitude = FMath::Max(0.0f, localForwardSpeed * liftAmount);
	FVector up = GetOwner()->GetActorUpVector();
	FVector liftForce = up * liftForceMagnitude;

	body->AddForce(liftForce);

	// Calculate downwards torque on plane body
		// Plane's rotation and location are tracked to cast a ray straight downwards
	FVector currentLocation = GetActorLocation(); // for raycasting
	FRotator currentRotation = GetActorRotation(); // Raycasting and downwards forces

	// Final value is the length of the ray. Needs to be as long as the plane can go high
	FVector EndLocation = currentLocation - FVector(0, 0, 1000000);

	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this); // Ray cast can't collide with the plane object

	// Cast a ray and return the hit result as a height value
	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		currentLocation,
		EndLocation,
		ECC_Visibility,
		CollisionParams
	);

	float HeightFromGround = 0.0f;
	if (bHit)
	{
		HeightFromGround = currentLocation.Z - HitResult.ImpactPoint.Z;
	}
	//DrawDebugLine(GetWorld(), currentLocation, HitResult.ImpactPoint, FColor::Green, false, -1, 0, 1.0f);
	UE_LOG(LogTemp, Warning, TEXT("height from ground: %f"), HeightFromGround);

	// TODO: This system doesn't take into account flying upsaide down (requiring reverse counter-steer) or sideways (requiring rudder counter-steer)

	// altitudeFactor is is scalable val from 0 to 1. 13 is the height read from the plane on the ground and 79920 is a conversion to 80k feet (cruising altitude for A10-C jets
	// THe idea is the plane will feel less downwards force as you get higher until you get into cruising altitude
	// When you're above cruising altitude, we fetch the absolute value of altitude factor so your plane starts dipping down again when you get too high
	if (currentRotation.Pitch > -80 && force.X > 100 && HeightFromGround > 15) { // if the plane isn't facing straight downwards
		float altitudeFactor = FMath::Abs(1.0f - ((HeightFromGround - 13.0f) / 79920.0f)); // scale torque power to altitude. Torque will be eliminated at heights of 80k feet
		//UE_LOG(LogTemp, Warning, TEXT("alt factor: %f"), altitudeFactor);

		// Clamp prevents odd error where torque "twitches" when you reach the specified value
		// 85 and 6 are scaling factors. 85 is general force and 6 scales with speed to decide how quickly the torque rises and drops. 8 means 0 at max speed
		// The clamping gives min and max torque force. Without it, the torque could go negative (bad)
		// Current implementation requires too much initial speed and goes down to the lower value too fast imo
			// 345 measures intensity. You could probably turn that down and the 10.f up to fix this? I don't want to spend too much time tweaking vals rn
		float torquePitch = FMath::Clamp((345 - (force.X / 10.0f)) * altitudeFactor, 100.f, 150.f); // 415 is tested to require almost perfect yoke resistance on controller. May need adjustment for sim
		UE_LOG(LogTemp, Warning, TEXT("torquepitch: %f"), torquePitch);
		FVector downTorque = FVector(torquePitch, 0.f, 0.f);
		body->AddTorqueInDegrees(downTorque, NAME_None, true);
	}

}

// Called to bind functionality to input
void ArefinedJet::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	//src: https://www.youtube.com/watch?v=O-3PNiTHlE0
	// Add input mapping context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller)) {
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())) {
			Subsystem->AddMappingContext(IM_main, 0);
		}
	}

	//Call Enhanced input value as Var input, cast into callback function for user input if valid
	if (UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		Input->BindAction(IA_stick, ETriggerEvent::Triggered, this, &ArefinedJet::stickControl);
		Input->BindAction(IA_throttle, ETriggerEvent::Triggered, this, &ArefinedJet::throttleControl);
		Input->BindAction(IA_rudder, ETriggerEvent::Triggered, this, &ArefinedJet::rudderControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Triggered, this, &ArefinedJet::landingGearControl);
		Input->BindAction(IA_toeBrake, ETriggerEvent::Triggered, this, &ArefinedJet::toeBrakeControl);
		Input->BindAction(IA_spoiler, ETriggerEvent::Triggered, this, &ArefinedJet::spoilerControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Completed, this, &ArefinedJet::landingGearControl);
	}
}

// Get stick control values and rotate jet appropriately
void ArefinedJet::stickControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //Controler input from a stick. Gets an x and y value to represent pitch and roll

	// Speed decides how easy or hard it is to turn
	// At speed = 0, you can't turn; at speed = 2000 it's very easy to turn; at speed = 4000 (max speed), it's very hard to turn but possible
	// Essentially, your plane is cutting through the air too fast to turn so you would need to slow down for it to work properly
	float torqueScale;
	if (force.X <= 2000.0f) {
		// Rising from 0 to 180 as speed goes from 0 to 2000
		torqueScale = FMath::Lerp(0.0f, 180.0f, force.X / 2000.0f);
	}
	else {
		// Falling from 180 to 20 as speed goes from 2000 to 4000
		torqueScale = FMath::Lerp(180.0f, 20.0f, (force.X - 2000.0f) / 2000.0f);
	}

	// !!!These values are backwards. Keep in mind when modifying in the future
	// Apply torque locally so that pitch may take roll into account
	FVector LocalTorque = FVector(-AxisValue.Y, -AxisValue.X, 0.0f) * torqueScale;
	torque = GetActorRotation().RotateVector(LocalTorque);
	UE_LOG(LogTemp, Warning, TEXT("Rotation is: %f,%f"), AxisValue.X, AxisValue.Y);

}

void ArefinedJet::throttleControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //A-10C jets are dual-engine. X=left engine, Y=Right engine
	UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), AxisValue.X, AxisValue.Y);

	//float forceMult = 500000;
	float forceVal = (AxisValue.X + AxisValue.Y);

	if (force.X <= 2000) {
		force += FVector(forceVal, forceVal, forceVal);
	}
	
	torque.Z += ((AxisValue.X - AxisValue.Y) / 15); //Y rotation is negative so we want to go negative if X is bigger. /15 to dampen plane's ability to turn. Bigger=more damp
}

void ArefinedJet::rudderControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>(); //Rudders will go between positive and negative to decide rotation so we don't need a vector

	torque.Z += FloatValue;
}

void ArefinedJet::landingGearControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	//UE_LOG(LogTemp, Warning, TEXT("Gear is: %f"), FloatValue);

	// Since this bool only calls when the button is pressed one way, we set it here and move the landing gear in tick
	// TODO: UNreal engine has built in suspension stuff. DO that for your langing gears
	//if (FloatValue == 1 and wheel_FR->GetRelativeLocation().Z) {
		//landingGearPos = (FVector(-185, -350, 110)); // Temp values until I make the final aircraft with the final positions
	//}
	//else {
		//landingGearPos = (FVector(-185, -350, 301));
		//if (moveSpeed > 10) {
			//float newMoveSPeed = moveSpeed -= 10;
			//moveSpeed = FMath::Lerp(moveSpeed, newMoveSPeed, 0.001);
		//}
	//}
}

// TODO: Map these values to buttons. toeBrakes should decrease velocity iff you're on the ground. Basically a lerp towards 0 on movement
	// But i'll have to make a ground collision checker. That'll also come in handy for the radial yoke movement setup stuff I wanna do
// The spoiler should dempen upwards movement on the plane AND speed regardless of height. It should make toe brakes more effective on the ground as the full weight of the plane is now on the wheels

void ArefinedJet::toeBrakeControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //2d Vector to represent both toe brakes. X = left Y = right
	//float toeBrake =Instance.GetValue().Get<float>();
	UE_LOG(LogTemp, Warning, TEXT("Left Toe brake: %f\n Right toe brake: %f"), AxisValue.X, AxisValue.Y);

	//float toeBrakeOut = ((AxisValue.X + 1) + (AxisValue.Y + 1));
	//UE_LOG(LogTemp, Warning, TEXT("ToeBrakeOut: %f"), toeBrakeOut);
	// TODO: Make a conditional that the plane's touching the ground and the landing gear is out for this to apply
	//float newMoveSPeed = moveSpeed -= toeBrakeOut;
	//moveSpeed = FMath::Lerp(moveSpeed, newMoveSPeed, 0.001); // When both toebrakes are applied this will lerp towards 0


}

// brake the plane from the wings. Good for landing plane. Increases effectiveness of toe brakes
void ArefinedJet::spoilerControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	//UE_LOG(LogTemp, Warning, TEXT("Spoiler is: %f"), FloatValue);
	//if (FloatValue == 1) {
		//moveSpeed = FMath::Lerp(moveSpeed, 0, 0.001); // According to my timer this almost halves the time it takes for braking (~47 to ~23)
		// TODO: Once I've implemented the downwards force, apply that here too and increase brake control power
	//}
}