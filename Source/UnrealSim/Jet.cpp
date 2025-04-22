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

// TODO: Enable physics in begin play so angular and linear damping work by default

const float ROTSPEED = 1.0f; //blanket rotation speed. Will eventually be decided via joystick input
float LandingGearY = -50.f;
int moveSpeed = 0; //Movement speed
float moveScaled = 0; // used to scale values that need to be stronger or weaker when flying/bussing
float maxMoveSpeed = 2000.f; // float so that when it's divided by moveSpeed I get a float output for moveScaled

//Tracked speed variables for interpolation of movement. I th
float pitch = 0; //up and down
float yaw = 0; //left and right
float roll = 0; //side to side
FVector forwardVector;

float ThrottleX;
float ThrottleY;

float Rudder; // 1 axis [-1,1] so only 1 float needed. Describes yaw ability of jet

float forwardDampVal = 0;

FVector landingGearPos(-185, -350, 110);

float traceLength = 10000000; // length of ray trace telling plane height in tick

// GPS screen location tracking details
FRotator DefaultGPSCamRotation;
FVector DefaultGPSCamTranslation;

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
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_body(TEXT("StaticMesh'/Game/Mesh/ShipBody.ShipBody'"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_Robot(TEXT("StaticMesh'/Game/Mesh/Robot.Robot'"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh>MeshAsset_wheel(TEXT("StaticMesh'/Game/Mesh/LandingGear.LandingGear'"));

	//Link components to meshes
	if (MeshAsset_body.Succeeded() and MeshAsset_wheel.Succeeded() and MeshAsset_Robot.Succeeded())
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
		wheel_FR->SetRelativeLocation(FVector(25, 15, -50)); //TODO: Define one fvector above and modify its values for the other two later
		wheel_FL->SetRelativeLocation(FVector(0, -30, 0));
		wheel_BM->SetRelativeLocation(FVector(-75, -15, 0));
	}
}

// TODO: If throttle is cancelled, nothing tells the system throttle is off and to lerp downwards. You commented that out

// Called when the game starts or when spawned
void AJet::BeginPlay()
{
	Super::BeginPlay();
	//int actorXLoc = GetActorLocation().X;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);

	// enable physics by default and set variables to body for phyics modifications
	body->SetSimulatePhysics(true); // TODO: This is a nice thought but it does NOT work. Maybe try setting these to this instead of body?

	if (PlayerController)
	{
		//Possess this actor
		PlayerController->Possess(this);
	}
	moveSpeed = 0; // fix from bug that sometimes starts moveSpeed at 1000. Probably mostly an xbox controller bug?
	DefaultGPSCamRotation = GPSCam->GetRelativeRotation(); // set gps camera to not rotate
	DefaultGPSCamTranslation = GPSCam->GetComponentLocation();
}

// Called every frame
void AJet::Tick(float DeltaTime)
{
	// Damping is set based on movement speed from both engines
	// a faster plane can catch air and so it's less affected by gravity
	forwardDampVal = (ThrottleX + ThrottleY) * 5;
	body->SetLinearDamping(forwardDampVal);
	body->SetAngularDamping(forwardDampVal);

	// Landing Gear position interp
	//FMath::FInterpTo(moveSpeed, 0, DeltaTime, 5.0f);
	if (wheel_FR) {
		FVector CurrentPosition = wheel_FR->GetRelativeLocation();
		FVector NewPosition = FMath::VInterpTo(CurrentPosition, landingGearPos, DeltaTime, 5.0f);
		wheel_FR->SetRelativeLocation(NewPosition);
	}

	// Set GPSCamera component to follow plane but never move upwards or tile upwards. Minimap
	if (GPSCam){
		FVector CurrentLocation = GPSCam->GetComponentLocation();
		FVector BodyLocation = body->GetComponentLocation();
		FRotator BodyRotation = body->GetComponentRotation();

		GPSCam->SetWorldRotation(FRotator(DefaultGPSCamRotation.Pitch, BodyRotation.Yaw, DefaultGPSCamRotation.Roll));
		GPSCam->SetWorldLocation(FVector(BodyLocation.X, BodyLocation.Y, DefaultGPSCamTranslation.Z));
	}

	// Plane's rotation and location are tracked to cast a ray straight downwards
	FVector currentLocation = GetActorLocation(); // for raycasting
	FRotator currentRotation = GetActorRotation(); // Raycasting and downwards forces

	// Final value is the length of the ray. Needs to be as long as the plane can go high
	FVector EndLocation = currentLocation - FVector (0, 0, 1000000);

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
	//UE_LOG(LogTemp, Warning, TEXT("pitch: %f"), currentRotation.Pitch);
	if (currentRotation.Pitch > -80 && moveSpeed > 100 && HeightFromGround > 400){ // if the plane isn't facing straight downwards
		float altitudeFactor = 1.0f - ((HeightFromGround - 80.0f) / 79920.0f); // scale torque power to altitude. Torque will be eliminated at heights of 80k feet

		// Clamp prevents odd error where torque "twitches" when you reach the specified value
		float torquePitch = FMath::Clamp((415.0f - (moveSpeed / 6.0f)) * altitudeFactor, 100.f, 500.f); // 415 is tested to require almost perfect yoke resistance on controller. May need adjustment for sim

		FVector torque = FVector(0.f, torquePitch, 0.f);

		body->AddTorqueInDegrees(torque, NAME_None, true);
	}

	// if plane isn't level then wind resistance will slow you down
	if (HeightFromGround > 400 && currentRotation.Pitch < -15 || currentRotation.Pitch>15) {
		float newMoveSPeed = moveSpeed -= 20;
		moveSpeed = FMath::Lerp(moveSpeed, newMoveSPeed, 0.001);
	}

	// scales movement for plane rotation to never surpass max speed
	moveScaled = moveSpeed/maxMoveSpeed;
	
	forwardVector = GetActorForwardVector();
	FVector NewLocation = GetActorLocation() + (forwardVector * moveSpeed * DeltaTime); // TODO: moveSPeed defaults to 1000 and idk
		
	UE_LOG(LogTemp, Warning, TEXT("rotspeed: %f"), ROTSPEED);
	UE_LOG(LogTemp, Warning, TEXT("NLX: %f, NLY: %f MoveSPeed: %i"), NewLocation.X, NewLocation.Y, moveSpeed);
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
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())) {
			Subsystem->AddMappingContext(IM_main, 0);
		}
	}

	//Call Enhanced input value as Var input, cast into callback function for user input if valid
	if (UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		Input->BindAction(IA_stick, ETriggerEvent::Triggered, this, &AJet::stickControl);
		Input->BindAction(IA_stick, ETriggerEvent::Completed, this, &AJet::stickRelease);
		Input->BindAction(IA_throttle, ETriggerEvent::Triggered, this, &AJet::throttleControl);
		Input->BindAction(IA_rudder, ETriggerEvent::Triggered, this, &AJet::rudderControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Triggered, this, &AJet::landingGearControl);
		Input->BindAction(IA_toeBrake, ETriggerEvent::Triggered, this, &AJet::toeBrakeControl);
		Input->BindAction(IA_spoiler, ETriggerEvent::Triggered, this, &AJet::spoilerControl);
		Input->BindAction(IA_landingGear, ETriggerEvent::Completed, this, &AJet::landingGearControl);
	}
}

//TODO: Lerp values don't reset when we cease to call the function. As such, throttle and roll only start to count down when we're calling them
// TODO: If I can find a way to read when the sticks are released and then set these vals to 0, that woudl be perfect
// We can do this in blueprint it MUST be possible in cpp

// Get stick control values and rotate jet appropriately
void AJet::stickControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //Controler input from a stick. Gets an x and y value to represent pitch and roll

	//UE_LOG(LogTemp, Warning, TEXT("Rotation is: %f,%f"), AxisValue.X, AxisValue.Y); // TODO: Create a widget to display this instead. TODO. Why is x y and y x?

	//Fix lerp from lagging when a player rotates their stick around from negative to positive
	pitch = FMath::Lerp(pitch, AxisValue.X, 0.1);
	if (AxisValue.X < -0.6 && pitch > 0 || AxisValue.X > 0.6 && pitch < 0){
		pitch = 0;
	}
	roll = FMath::Lerp(roll, AxisValue.Y, 0.1);
	if (AxisValue.Y < -0.6 && roll > 0 || AxisValue.Y > 0.6 && roll < 0){
		roll = 0;
	}
	//UE_LOG(LogTemp, Warning, TEXT("pitch(Y): %f, roll(X): %f"), pitch, roll); // Movement is updating correctly??? Something updating the model must br wrong

	FRotator currentRotation = GetActorRotation();
	FRotator NewInputRotation = FRotator(ROTSPEED * pitch * moveScaled, 0.0f, ROTSPEED * roll * moveScaled);
	AddActorLocalRotation(NewInputRotation);
}

// Reset stick positions when player releases the stick. Fixes lerp lag
void AJet::stickRelease() { //reset pitch and roll
	//UE_LOG(LogTemp, Warning, TEXT("Sticks Reset"));
	roll = 0;
	pitch = 0;
}

void AJet::throttleControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //A-10C jets are dual-engine. X=left engine, Y=Right engine
	UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), AxisValue.X, AxisValue.Y);
	
	// TODO: This all works but it should (at least the else if) be moved to tick so it runs when the runction doesn't
	if ((ThrottleX + ThrottleY) >= (AxisValue.X + AxisValue.Y)){ // Throttle is moving
		UE_LOG(LogTemp, Warning, TEXT("----------------axis x less than 0. Throttle is lerping downwards"));
		ThrottleX = FMath::FInterpTo(ThrottleX, AxisValue.X, GetWorld()->GetDeltaSeconds(), 0.01);
		ThrottleY = FMath::FInterpTo(ThrottleY, AxisValue.Y, GetWorld()->GetDeltaSeconds(), 0.01);
	
	}
	else if ((ThrottleX + ThrottleY) < (AxisValue.X + AxisValue.Y)){
		UE_LOG(LogTemp, Warning, TEXT("----------------axis x greater than 0. Throttle is lerping upwards"));
		ThrottleX = FMath::FInterpTo(ThrottleX, AxisValue.X, GetWorld()->GetDeltaSeconds(), 0.5);
		ThrottleY = FMath::FInterpTo(ThrottleY, AxisValue.Y, GetWorld()->GetDeltaSeconds(), 0.5);
	}

	//UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), ThrottleX, ThrottleY);

	moveSpeed = (ThrottleX * 1000 + ThrottleY * 1000);

	float rotationSpeed = ((ThrottleX - ThrottleY)/15); //Y rotation is negative so we want to go negative if X is bigger. /15 to dampen plane's ability to turn. Bigger=more damp

	FRotator newRot = FRotator(0.0f, ROTSPEED * rotationSpeed, 0.0f); //TODO: Call ruddersControl and pass float value in instead. RuddersControl takes a different value type. Make a new function that they BOTH call that takes a float made from the new type
	AddActorLocalRotation(newRot);
}

void AJet::rudderControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>(); //Rudders will go between positive and negative to decide rotation so we don't need a vector
	//UE_LOG(LogTemp, Warning, TEXT("Yaw is: %f"), FloatValue);

	if (FloatValue < 0.09 && FloatValue > -0.08){
		Rudder = 0;
	}
	Rudder = FMath::Lerp(Rudder, FloatValue, 0.01);



	FRotator newRot = FRotator(0.0f, ROTSPEED * Rudder * moveScaled, 0.0f);
	AddActorLocalRotation(newRot);
}

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
		if (moveSpeed > 10) {
			float newMoveSPeed = moveSpeed -= 10;
			moveSpeed = FMath::Lerp(moveSpeed, newMoveSPeed, 0.001);
		}
	}
}

// TODO: Map these values to buttons. toeBrakes should decrease velocity iff you're on the ground. Basically a lerp towards 0 on movement
	// But i'll have to make a ground collision checker. That'll also come in handy for the radial yoke movement setup stuff I wanna do
// The spoiler should dempen upwards movement on the plane AND speed regardless of height. It should make toe brakes more effective on the ground as the full weight of the plane is now on the wheels

void AJet::toeBrakeControl(const FInputActionInstance& Instance){
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //2d Vector to represent both toe brakes. X = left Y = right
	//float toeBrake =Instance.GetValue().Get<float>();
	UE_LOG(LogTemp, Warning, TEXT("Left Toe brake: %f\n Right toe brake: %f"), AxisValue.X, AxisValue.Y);

	float toeBrakeOut = ((AxisValue.X+1) + (AxisValue.Y+1));
	UE_LOG(LogTemp, Warning, TEXT("ToeBrakeOut: %f"), toeBrakeOut);
	// TODO: Make a conditional that the plane's touching the ground and the landing gear is out for this to apply
	float newMoveSPeed = moveSpeed -= toeBrakeOut;
	moveSpeed = FMath::Lerp(moveSpeed, newMoveSPeed, 0.001); // When both toebrakes are applied this will lerp towards 0


}

// brake the plane from the wings. Good for landing plane. Increases effectiveness of toe brakes
void AJet::spoilerControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	//UE_LOG(LogTemp, Warning, TEXT("Spoiler is: %f"), FloatValue);
	if (FloatValue == 1){
		moveSpeed = FMath::Lerp(moveSpeed, 0, 0.001); // According to my timer this almost halves the time it takes for braking (~47 to ~23)
		// TODO: Once I've implemented the downwards force, apply that here too and increase brake control power
	}
}


//TODO: IA should probably be imported directly into file instead of doing it via blueprints