// Fill out your copyright notice in the Description page of Project Settings.


#include "Jet.h"
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

const float MAX_SPEED = 2000; // ideally will be 4000 later but speeds were optimized around 2000 as of now

//float turnAmount;// TODO: these are constant multipliers. I should find good vals
float throttlePos = 0; // Track throttle. Value between 0 and 2

// Arbitrary multipliers to make sure handling feels good but realistic
float throttleAmount = 800000;
float turnAmount = 10;
float liftAmount = 4000;
int torqueScalar = 20; // torque for stick and rudders divided by this value to make the forces feel better and more realistic
const float DampingStrength = 500.0f;
float traceLength = 10000000; // length of ray trace telling plane height in tick

// Changing globals used in functions
FVector landingGearPos(-185, -350, 110);


// Component positional tracking

// GPS screen location tracking details
FRotator DefaultGPSCamRotation;
FVector DefaultGPSCamTranslation;

//float throttle, pitch, yaw, roll;
FVector torque; //Pitch, roll, yaw (in that order) movement on plane // TODO: Multiply by a float that's between 0 and 1 and set by force so a still plane can't steer
//float force = 0; // forward force on Jet plane (from throttle)

// Sets default values
AJet::AJet()
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
void AJet::BeginPlay()
{
	Super::BeginPlay();

	body->SetSimulatePhysics(true);
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (PlayerController)
	{
		//Possess this actor
		PlayerController->Possess(this);
	}

	// Prevents bug where numbers are obscenely high on startup
	torque = FVector(0, 0, 0);
	force = 0;

	// Defaults I built physics around
	body->SetLinearDamping(5);
	body->SetAngularDamping(10);

	DefaultGPSCamRotation = GPSCam->GetRelativeRotation(); // set gps camera to not rotate
	DefaultGPSCamTranslation = GPSCam->GetComponentLocation();
}

// TODO: Lerp values towards 0 once they're not being used anymore. Or find some constant value that they have to fight against to represent wind

// Called every frame
void AJet::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Apply forces to jet from yoke feedback. Forces calculated inside of user input functions below
	body->AddTorqueInDegrees(torque, NAME_None, true);

	// clamp speed value to throttle position value between the min and max of both
	float targetForceMag = FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 2.0f), FVector2D(0.0f, MAX_SPEED), throttlePos);

	// interpSpeed based on if shifting up or down
	float interpSpeed;
	if (force > targetForceMag) {
		interpSpeed = 0.05;
	}
	else {
		interpSpeed = 1;
	}
	force = FMath::FInterpTo(force, targetForceMag, DeltaTime, interpSpeed);

	FVector appliedForce = GetActorForwardVector() * force * 800000;

	body->AddForce(appliedForce);
	UE_LOG(LogTemp, Warning, TEXT("Force: %f"), force);

	//Calculate Lift
	FVector forward = GetOwner()->GetActorForwardVector();
	FVector velocity = body->GetPhysicsLinearVelocity();
	float localForwardSpeed = FVector::DotProduct(velocity, forward);

	float liftForceMagnitude = FMath::Max(0.0f, localForwardSpeed * liftAmount);
	FVector up = GetOwner()->GetActorUpVector();
	FVector liftForce = up * liftForceMagnitude;

	body->AddForce(liftForce);

	// Set wheel to move up or down into position based on landing gear switch position
	if (wheel_FR) {
		FVector CurrentPosition = wheel_FR->GetRelativeLocation();
		FVector NewPosition = FMath::VInterpTo(CurrentPosition, landingGearPos, DeltaTime, 5.0f);
		wheel_FR->SetRelativeLocation(NewPosition);
	}

	// Set GPSCamera component to follow plane but never move upwards or tile upwards. Minimap
	if (GPSCam) {
		FVector CurrentLocation = GPSCam->GetComponentLocation();
		FVector BodyLocation = body->GetComponentLocation();
		FRotator BodyRotation = body->GetComponentRotation();

		GPSCam->SetWorldRotation(FRotator(DefaultGPSCamRotation.Pitch, BodyRotation.Yaw, DefaultGPSCamRotation.Roll));
		GPSCam->SetWorldLocation(FVector(BodyLocation.X, BodyLocation.Y, DefaultGPSCamTranslation.Z));
	}

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

	if (bHit)
	{
		HeightFromGround = currentLocation.Z - HitResult.ImpactPoint.Z;
	}
	//DrawDebugLine(GetWorld(), currentLocation, HitResult.ImpactPoint, FColor::Green, false, -1, 0, 1.0f);
	UE_LOG(LogTemp, Warning, TEXT("height from ground: %f"), HeightFromGround);

	// altitudeFactor is is scalable val from 0 to 1. 13 is the height read from the plane on the ground and 79920 is a conversion to 80k feet (cruising altitude for A10-C jets
	// THe idea is the plane will feel less downwards force as you get higher until you get into cruising altitude
	// When you're above cruising altitude, we fetch the absolute value of altitude factor so your plane starts dipping down again when you get too high
	if (currentRotation.Pitch > -80 && force > 100 && HeightFromGround > 15) { // if the plane isn't facing straight downwards
		float altitudeFactor = FMath::Abs(1.0f - ((HeightFromGround - 13.0f) / 79920.0f)); // scale torque power to altitude. Torque will be eliminated at heights of 80k feet

		// The clamping gives min and max torque force. Without it, the torque could go negative (bad)
		// Current implementation requires too much initial speed and goes down to the lower value too fast imo
			// 345 measures intensity. You could probably turn that down and the 10.f up to fix this? I don't want to spend too much time tweaking vals rn
		float torquePitch = FMath::Clamp((345 - (force / 10.0f)) * altitudeFactor, 100.f, 150.f);
		// Get the plane's current right vector in world space
		FVector Forward = GetActorForwardVector();  // Local +X in world space
		FVector WorldDown = FVector(0.f, 0.f, -1.f); // Global down direction

		// Compute axis that would rotate Forward toward WorldDown
		FVector PitchDownAxis = FVector::CrossProduct(Forward, WorldDown).GetSafeNormal();

		// Apply torque around that axis
		FVector pitchTorque = PitchDownAxis * torquePitch;
		body->AddTorqueInDegrees(pitchTorque, NAME_None, true);
	}

	// Area of attack. If plane isn't flying straight then speed will begin to reduce
	if (HeightFromGround > 50 && force > 0 && currentRotation.Pitch < -15 || currentRotation.Pitch>15) {
		force -= 5; // small value. This shouldn't be that noticable or detrimental but it still has an effect
	}

	// Apply damping on torque to act as a normalizing force on pitch and yaw
	if (!torque.IsNearlyZero(0.01f)) // Avoid tiny jitter
	{
		FVector dampingTorque = -torque.GetSafeNormal() * FMath::Min(torque.Length(), DampingStrength);
		torque += dampingTorque * DeltaTime;
	}
}

// Called to bind functionality to input
void AJet::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
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
		Input->BindAction(IA_stick, ETriggerEvent::Triggered, this, &AJet::stickControl);
		Input->BindAction(IA_throttle, ETriggerEvent::Triggered, this, &AJet::throttleControl);
		Input->BindAction(IA_rudder, ETriggerEvent::Triggered, this, &AJet::rudderControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Triggered, this, &AJet::landingGearControl);
		Input->BindAction(IA_toeBrake, ETriggerEvent::Triggered, this, &AJet::toeBrakeControl);
		Input->BindAction(IA_spoiler, ETriggerEvent::Triggered, this, &AJet::spoilerControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Completed, this, &AJet::landingGearControl);
	}
}

// Get stick control values and rotate jet appropriately
void AJet::stickControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //Controler input from a stick. Gets an x and y value to represent pitch and roll

	// Speed decides how easy or hard it is to turn
	// At speed = 0, you can't turn; at speed = 2000 it's very easy to turn; at speed = 4000 (max speed), it's very hard to turn but possible
	// Essentially, your plane is cutting through the air too fast to turn so you would need to slow down for it to work properly

	float torqueScale;
	if (force <= 2000.0f) {
		torqueScale = FMath::Lerp(0.0f, 180.0f, force / (MAX_SPEED / 2));
	}
	else {
		torqueScale = FMath::Lerp(180.0f, 20.0f, (force - (MAX_SPEED / 2)) / (MAX_SPEED / 2));
	}

	// Apply torque locally so that pitch may take roll into account
	FVector LocalTorque = FVector(-AxisValue.Y, -AxisValue.X, 0.0f) * torqueScale;
	torque += (GetActorRotation().RotateVector(LocalTorque)) / torqueScalar;
	UE_LOG(LogTemp, Warning, TEXT("Rotation is: %f,%f"), AxisValue.X, AxisValue.Y);

}

// Save speed based on throttle position to calculate plane velocity. Overwrite rotation if one engine is lower than the other
void AJet::throttleControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //A-10C jets are dual-engine. X=left engine, Y=Right engine
	UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), AxisValue.X, AxisValue.Y);

	//Calculating throttle here and then applying it in tick so we can shift speed up and down
	throttlePos = (AxisValue.X + AxisValue.Y);

	// Calculate turning power of jet based on engine control instead of rudders. Turning this way should be slow and clunky
	torque.Z += ((AxisValue.X - AxisValue.Y) / 15); //Y rotation is negative so we want to go negative if X is bigger. /15 to dampen plane's ability to turn. Bigger=more damp
}

// Control rudder pedals under jet. Steer plane on yaw axis
void AJet::rudderControl(const FInputActionInstance& Instance) { // TODO: Make local
	float floatValue = Instance.GetValue().Get<float>();

	// Scale the ability of the torque based on speed so that the plane is more capable of turning at higher forces
	float torqueScale;
	if (force <= 2000.0f) {
		torqueScale = FMath::Lerp(0.0f, 180.0f, force / MAX_SPEED);
	}
	else {
		torqueScale = FMath::Lerp(180.0f, 20.0f, (force - MAX_SPEED) / MAX_SPEED);
	}

	// Global yaw axis based on current rotation
	FVector globalYawAxis = GetActorRotation().RotateVector(FVector(0.0f, 0.0f, 1.0f));

	float groundScale = (HeightFromGround < 40) ? torqueScale * 2.0f : torqueScale;

	// Accumulate torque instead of overwriting it
	torque += (globalYawAxis * floatValue * groundScale) / torqueScalar;
}

// Moves the landing gears in and out.
// A-10C jets have strong landing gears that can even be extended while the plane is on the ground so this can be really simple
void AJet::landingGearControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	//UE_LOG(LogTemp, Warning, TEXT("Gear is: %f"), FloatValue);

	// Since this bool only calls when the button is pressed one way, we set it here and move the landing gear in tick
	// TODO: UNreal engine has built in suspension stuff. DO that for your langing gears
	if (FloatValue == 1 and wheel_FR->GetRelativeLocation().Z) {
		landingGearPos = (FVector(-185, -350, 110)); // Temp values until I make the final aircraft with the final positions
	}
	else {
		landingGearPos = (FVector(-185, -350, 301));
		if (HeightFromGround <= 40 && force > 0) { // if the landing gear is out, it increases drag force. Not applicable from ground
			force -= 10;
		}
	}
}


// Wheel brakes. They only work when on the ground
void AJet::toeBrakeControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //2d Vector to represent both toe brakes. X = left Y = right

	UE_LOG(LogTemp, Warning, TEXT("Left Toe brake: %f\n Right toe brake: %f"), AxisValue.X, AxisValue.Y);

	float brakeForce = (AxisValue.X + AxisValue.Y) * 10;

	if (HeightFromGround <= 40 && force > 0) {
		force -= brakeForce;

	}
	if (force < 0) {
		force = 0;
	}
}

// brake the plane from the wings. Works on ground and in air
void AJet::spoilerControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();

	float brakeForce = FloatValue * 10;

	if (force > 0) {
		force -= brakeForce;
	}
	if (force < 0) {
		force = 0;
	}
}