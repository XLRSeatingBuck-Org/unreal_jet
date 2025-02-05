// Fill out your copyright notice in the Description page of Project Settings.


#include "Jet.h"
#include <cmath> //contains interpolation function
#include "Components/StaticMeshComponent.h" //Create default subobject
#include "Camera/CameraComponent.h"
#include "UObject/ConstructorHelpers.h" //Set up file paths for loading in meshes
#include "InputMappingContext.h" //Setups for enhanced input
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h" //Posession for player

// TODO: Enable physics in begin play so angular and linear damping work by default

const float ROTSPEED = 10.0f; //blanket rotation speed. Will eventually be decided via joystick input
float LandingGearY = -50.f;
int moveSpeed = 0; //Movement speed
//float gravity = -1;
float moveScaled = 0; // used to scale values that need to be stronger or weaker when flying/bussing
float maxMoveSpeed = 2000.f; // float so that when it's divided by moveSpeed I get a float output for moveScaled

//Tracked speed variables for interpolation of movement. I th
float pitch = 0; //up and down
float yaw = 0; //left and right
float roll = 0; //side to side
// TODO: Make throttleX and Y 1 vector
FVector ForwardVector;

float ThrottleX;
float ThrottleY;

float Rudder; // 1 axis [-1,1] so only 1 float needed. Describes yaw ability of jet

int counter = 0; // todo: Counter of what??
float forwardDampVal = 0;

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
	moveSpeed = 0; // fix from bug that sometimes starts moveSpeed at 1000. Probably mostly an xbox controller bug?
	
}

// Called every frame
void AJet::Tick(float DeltaTime)
{

	//I take throttle values x and y. Add them together
	// Multiply this new val by 3
	// apply it to linear and angular damping
	forwardDampVal = (ThrottleX + ThrottleY) * 3;

	body->SetLinearDamping(forwardDampVal);
	body->SetAngularDamping(forwardDampVal);

	if (forwardDampVal < 5){ // If plane isn't going near top speed then we should start tilting downwards
		// plane body rotates downwards
		// set rotation relative to forward damp value where 5 requires no rotation and 1 should lerp towards 180
			// 180/5 is 36
			// for now I should just get the plane to rotate down to 180 degrees and then add the lerp vals from there
		
		//AddActorRotation(180);

		// get actor rotation
			// add value to it until it's 180 facing straight down
		
		FRotator CurrentRotation = GetActorRotation();
		UE_LOG(LogTemp, Warning, TEXT("---------------------------------------Current Rotation: %f---------------------------"), CurrentRotation.Yaw);
		// values are p, y, and r. Fully down is y=0. y should be lerpint to 0
		FRotator NewInputRotation = FRotator(0.f, 0.f, 0.f);
		//AddActorLocalRotation(NewInputRotation);
		ThrottleY = FMath::FInterpTo(CurrentRotation.Yaw, 0, GetWorld()->GetDeltaSeconds(), 300.0f);
		// lerp plane rotation to 0 IFF plane speed is high enough and plane is physically in air
	}
	UE_LOG(LogTemp, Warning, TEXT("damp val speed = %f"), forwardDampVal);

	if (ThrottleX == 0 && ThrottleY == 0){
		UE_LOG(LogTemp, Warning, TEXT("speed should be lerping to 0"));
    	moveSpeed = FMath::FInterpTo(moveSpeed, 0, DeltaTime, 5.0f);
	}

	moveScaled = moveSpeed/maxMoveSpeed; // moveScaled is now a float between 0 and 1
	UE_LOG(LogTemp, Warning, TEXT("scaled: %f moveSPeed = %i max = %f"), moveScaled, moveSpeed, maxMoveSpeed);
	
	// Get the forward vector of the actor
	ForwardVector = GetActorForwardVector();

	// Calculate the new location based on forward movement
	FVector NewLocation = GetActorLocation() + (ForwardVector * moveSpeed * DeltaTime); // TODO: moveSPeed defaults to 1000 and idk
		
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
		// Get local player subsystem
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer())) {
			// Add input context
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

	// Jets "cut" through the air. Instead of applying rotational forces we should be tilting the plane to match the stick
		// IE: if the stick is tilted 10 degrees the plane is tilted 10 degrees. WHen you ease off the plane straightens out
		// if the plane isn't going fast enough and loses altitude you're no longer cutting though the wind and so your plane begins to tilt downwards

	UE_LOG(LogTemp, Warning, TEXT("Rotation is: %f,%f"), AxisValue.X, AxisValue.Y); // TODO: Create a widget to display this instead. TODO. Why is x y and y x?

	//Fix lerp from lagging when a player rotates their stick aroudn from negative to positive
	pitch = FMath::Lerp(pitch, AxisValue.X, 0.1);
	if (AxisValue.X < -0.6 && pitch > 0 || AxisValue.X > 0.6 && pitch < 0){ // Fixes lerp doing extra steering while it goes to appropriate val
		pitch = 0;
	}
	roll = FMath::Lerp(roll, AxisValue.Y, 0.1);
	if (AxisValue.Y < -0.6 && roll > 0 || AxisValue.Y > 0.6 && roll < 0){ // Fixes lerp doing extra steering while it goes to appropriate val
		roll = 0;
	}
	UE_LOG(LogTemp, Warning, TEXT("pitch(Y): %f, roll(X): %f"), pitch, roll); // Movement is updating correctly??? Something updating the model must br wrong

	FRotator CurrentRotation = GetActorRotation();
	FRotator NewInputRotation = FRotator(ROTSPEED * pitch * moveScaled, 0.0f, ROTSPEED * roll * moveScaled);
	AddActorLocalRotation(NewInputRotation);
}

// Reset stick positions when player releases the stick
void AJet::stickRelease() { //reset pitch and roll
	UE_LOG(LogTemp, Warning, TEXT("Sticks Reset"));
	roll = 0;
	pitch = 0;
}

void AJet::throttleControl(const FInputActionInstance& Instance) {
	FVector2D AxisValue = Instance.GetValue().Get<FVector2D>(); //A-10C jets are dual-engine. X=left engine, Y=Right engine
	UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), AxisValue.X, AxisValue.Y);
	
	if (AxisValue.X > 0){ // Throttle is moving
		UE_LOG(LogTemp, Warning, TEXT("axis x greater than 0"));
		ThrottleX = FMath::FInterpTo(ThrottleX, AxisValue.X, GetWorld()->GetDeltaSeconds(), 3.0f);
		ThrottleY = FMath::FInterpTo(ThrottleY, AxisValue.Y, GetWorld()->GetDeltaSeconds(), 3.0f);
	}
	else{ // Throttle is down. The plane should be decellerating
		UE_LOG(LogTemp, Warning, TEXT("axis value not greater than 0"));
    	ThrottleX = FMath::FInterpTo(ThrottleX, AxisValue.X, GetWorld()->GetDeltaSeconds(), 300.0f);
    	ThrottleY = FMath::FInterpTo(ThrottleY, AxisValue.Y, GetWorld()->GetDeltaSeconds(), 300.0f);
	}
	

	//UE_LOG(LogTemp, Warning, TEXT("Throttle left engine: %f\n Throttle right engine: %f"), ThrottleX, ThrottleY);

	moveSpeed = (ThrottleX * 1000 + ThrottleY * 1000);

	float rotationSpeed = ((ThrottleX - ThrottleY)/15); //Y rotation is negative so we want to go negative if X is bigger. /15 to dampen plane's ability to turn. Bigger=more damp

	FRotator newRot = FRotator(0.0f, ROTSPEED * rotationSpeed, 0.0f); //TODO: Call ruddersControl and pass float value in instead. RuddersControl takes a different value type. Make a new function that they BOTH call that takes a float made from the new type
	AddActorLocalRotation(newRot);
}

void AJet::rudderControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>(); //Rudders will go between positive and negative to decide rotation so we don't need a vector
	UE_LOG(LogTemp, Warning, TEXT("Yaw is: %f"), FloatValue);

	if (FloatValue < 0.09 && FloatValue > -0.08){
		Rudder = 0;
	}
	Rudder = FMath::Lerp(Rudder, FloatValue, 0.01);



	FRotator newRot = FRotator(0.0f, ROTSPEED * Rudder * moveScaled, 0.0f);
	AddActorLocalRotation(newRot);
}

void AJet::landingGearControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	UE_LOG(LogTemp, Warning, TEXT("Gear is: %f"), FloatValue);
	//Timeout zone. Something here causes crashes and I don't want to spend time on it rn
	// Also note that I got rid of one of the calls to landing gear. Why were there 2???
	if (FloatValue == 1 and wheel_FR->GetRelativeLocation().Z) {
		wheel_FR->SetRelativeLocation(FVector(225, 341, 600.f)); // Temp values until I make the final aircraft with the final positions
	}
	else {
		wheel_FR->SetRelativeLocation(FVector(225, 341, -100));
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
	moveSpeed = FMath::Lerp(moveSpeed, moveSpeed * toeBrakeOut, 0.001); // When both toebrakes are applied this will lerp towards 0


}

// brake the plane from the wings. Good for landing plane. Increases effectiveness of toe brakes
void AJet::spoilerControl(const FInputActionInstance& Instance) {
	float FloatValue = Instance.GetValue().Get<float>();
	UE_LOG(LogTemp, Warning, TEXT("Spoiler is: %f"), FloatValue);
	if (FloatValue == 1){
		moveSpeed = FMath::Lerp(moveSpeed, 0, 0.001); // According to my timer this almost halves the time it takes for braking (~47 to ~23)
		// TODO: Once I've implemented the downwards force, apply that here too and increase brake control power
	}
}


//TODO: IA should probably be imported directly into file instead of doing it via blueprints