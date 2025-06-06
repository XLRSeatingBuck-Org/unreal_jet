# Technical Documentation

# Architecture and Design

See our website ([car](https://xlrseatingbuck-org.github.io/unity-car.html)) ([plane](https://xlrseatingbuck-org.github.io/unity-plane.html)) for more information.

![main activity diagram](images/main_activity.drawio.png)

## Project Overview

This simulation is set in a realistic 3D environment constructed using Cesium for Unity, allowing for accurate geospatial data integration. The environment was set up following the official Cesium documentation, enabling real-world terrain and building placement. The simulation supports both VR and non-VR modes, allowing users to switch seamlessly between the two.

## Input

![input diagram](images/input-diagram.png)

Input in Unity is abstracted into generic actions that are read out as a data type (bool for button, float for slider, etc). This way, the code doesn't have to worry about specific devices or combinations of devices. Mapping controllers to actions is as simple as pressing a button on the controller and listening for said button.

## Enviroment

### Car

We used ArcGIS CityEngine to extract a region of Corvallis. This region includes the location, size, and shape of buildings, trees, and roads. This data was exported as an fbx object and textures, and then imported into Unity. Cesium's Bing map data provides the visuals for the rest of the world, to ground the city model in it's location.

### Plane

We use Cesium's Bing terrain and Google 3d tiles to render the world that the user flies over. The 3d tiles are restricted to just the airport area to make it stand out more and to improve performance.

Cesium for Unity does not support vector or point data, so for the forest where the fire is, we used an OSM extract of the area and wrote a custom xml parser to extract the relevant data and place trees in the correct areas. This was done instead of using Google's 3d data because each tree needs to ability to individually die from the fire, which is not possible with only mesh data from Google.

# Classes

![main class diagram](images/class_diagram.drawio.png)

## TitleController

Simple ui logic for handling button presses to go to the correct scene from the starting scene.

## ExperienceDirector

The top-level class for the entire project. Stores the general state of the simulation and determines what should happen next. It also tracks win/loss conditions and handles restarting or exiting the game.

## OsmLoader

Loads the OSM data, which contains information about specific points on the map (mainly buildings and forest).
This is used to place trees in the proper area, that can then be lit on fire.

## BeaconTrigger

Tracks whether a vehicle is in range of a given marker. Reports this state to the ExperienceDirector to change the status text.

## FireExtinguishTracker

Tracks the extinguished state of each fire in the simulation. Communicates with the ExperienceDirector to determine if all fires have been put out.

## CrashController

Manages collisions between the vehicle and the environment. Triggers haptic feedback to enhance realism upon impact.

## CarMovement

Main movement class for the firetruck. Gets input from controllers and applies forces to the truck via its wheels.

## JetMovement

Main movement class for the aeriel firefighter.
Gets input from controllers and applies forces to the plane. Handles all of the flight physics, including
simulating drag, calculating lift based on area of attack, and controlling steering/flaps.
Finally, this applies basic haptic vibration depending on the speed of the plane.

## HoseController

Enables movement and aiming of the firetruck hose. Controls water spray to extinguish fires.

## EnableExteng

Handles the releasing of foam from the plane.

## BringFireDown

Handles collision between water and fire, and properly extinguishing the fire.
This signals to FireExtinguishTracker when the fire is extinguished.
This also grows the fire over time, triggering a loss when it gets too big.

## GasCollision

Handles shooting foam out of the plane.
Extinguishes the fire when the foam touches the fire.
This signals to FireExtinguishTracker when the fire is extinguished.

## IncreaseFire

Similar to the behavior of BringFireDown, this grows the fire over time, triggering a loss when it gets too big.

## CameraController

Handles camera switching based on whether the user is in VR or not.
Also changes the UI appropriately to be visible in VR.

# Building and Releasing

1. Open Unity
2. Go to File > Build Profiles
3. Go to the Windows platform and click Build
4. Make a zip file from the build folder you just built to
5. [Create a new release on the repository](https://github.com/XLRSeatingBuck-Org/unity-car-jet/releases/new)

# Testing

This mirrors each item of the main activity diagram.
Everything works in the current build. If anything breaks, refer back to these cases.

| Test Case | Acceptance Criteria | Succeeds? |
| --- | --- | --- |
| TC-01 | Player vehicle starts at airport or fire station | ✓ |
| TC_02 | Fire beacon exists at fire | ✓ |
| TC_03 | Player vehicle can use controls to take off airplane or leave fire station | ✓ |
| TC_04 | Player vehicle can use controls to aim fire hose on fire truck | ✓ |
| TC_05 | Player vehicle can use controls to fire water/foam from the fire plane or fire truck | ✓ |
| TC_06 | Fire expands over time | ✓ |
| TC_07 | Player loses if fire gets too big (i.e. doubles in size) | ✓ |
| TC_08 | Fire reduces when in contact with water/foam | ✓ |
| TC_09 | Extinguishing all the fires instructs player to fly/drive home | ✓ |
| TC_10 | Player vehicle can use controls to fly/drive back home | ✓ |
| TC_11 | Crashing player vehicle into a surface results in a loss | ✓ |
| TC_12 | Player vehicle can use controls to land at the airport or pull into fire station | ✓ |

# Future Enhancements

In its current state, this project achieves all main goals given to us.

Below is further potential functionality that may be added by future developers:

- Visuals and realism could be vastly improved via better assets and more fine tuning of post processing effects.
- Fire simulation can be expanded to be more realistic. For exmaple, it could destroy trees and buildings as it spreads.
- Additional scenarios and factors can be added, such as weather events and turbulence.
- Biometrics could be implemented into the system to read heartrate and potentially adjust difficulty to make a more pleasant experience.
- The simulation could be connected to a multi-screen or multi-projector display for more immersion.
- The VR mode could be expanded upon to allow controller input and cockpit interaction.
