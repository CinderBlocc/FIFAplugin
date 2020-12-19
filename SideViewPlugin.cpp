#include "SideViewPlugin.h"
#include "bakkesmod\wrappers\includes.h"
#include <sstream>
#include <filesystem>

using namespace std;

BAKKESMOD_PLUGIN(SideViewPlugin, "FIFA Camera", "1.3", PLUGINTYPE_FREEPLAY)

void SideViewPlugin::onLoad()
{
	Initialize();
	cvarManager->registerNotifier("FIFAEnable", [this](std::vector<string> params){Enable();}, "Enables FIFA camera", PERMISSION_ALL);
	cvarManager->registerNotifier("FIFADisable", [this](std::vector<string> params){Disable();}, "Disables FIFA camera", PERMISSION_ALL);

	cvarManager->registerCvar("FIFA_Vertical", "500", "Set the vertical position of the camera", true, true, -2000, true, 2000);
	cvarManager->registerCvar("FIFA_Horizontal", "-400", "Set the horizontal position of the camera", true, true, -2000, true, 2000);
	cvarManager->registerCvar("FIFA_Angle", "-40", "Set the angle of the camera", true, true, -89, true, 89);
	cvarManager->registerCvar("FIFA_Indicator", "1", "Toggle visibility of position indicator");
}


void SideViewPlugin::CreateValues()
{
	float vertical = cvarManager->getCvar("FIFA_Vertical").getFloatValue();
	float horizontal = cvarManager->getCvar("FIFA_Horizontal").getFloatValue();
	float angle = cvarManager->getCvar("FIFA_Angle").getFloatValue();
		
	CarWrapper car = gameWrapper->GetLocalCar();
	CameraWrapper camera = gameWrapper->GetCamera();
	if(!car.IsNull() && !camera.IsNull())
	{
		BallWrapper ball = GetCurrentGameState().GetBall();
		if(isInBallCam && !ball.IsNull())
		{
			Vector ballLocation = ball.GetLocation();

			float Zposition = 0;
			if(ballLocation.Z < vertical)
				Zposition = vertical;
			else
				Zposition = ballLocation.Z;
			FOCUS = Vector{ballLocation.X + horizontal, ballLocation.Y, Zposition};
		}
		else
		{
			Vector carLocation = car.GetLocation();

			float Zposition = 0;
			if(carLocation.Z < vertical)
				Zposition = vertical;
			else
				Zposition = carLocation.Z;
			FOCUS = Vector{carLocation.X + horizontal, carLocation.Y, Zposition};
		}

		ProfileCameraSettings camSettings = camera.GetCameraSettings();
		ROTATION = Rotator{(int)(angle * 182.044449), 0, 0};	
		DISTANCE = camSettings.Distance * 6;
		FOV = camSettings.FOV;
	}
	else if(car.IsNull() && !camera.IsNull())
	{
		//Let the goal replay be normal
		overrideValue[0] = false;//Focus X
		overrideValue[1] = false;//Focus Y
		overrideValue[2] = false;//Focus Z
		overrideValue[3] = false;//Rotation Pitch
		overrideValue[4] = false;//Rotation Yaw
		overrideValue[5] = false;//Rotation Roll
		overrideValue[6] = false;//Distance
		overrideValue[7] = false;//FOV
	}
}

void SideViewPlugin::Render(CanvasWrapper canvas)
{
	//canvas.Project is disabled during online play, so the indicator may look strange

	if(CanCreateValues() && cvarManager->getCvar("FIFA_Indicator").getBoolValue())
	{
		CarWrapper car = gameWrapper->GetLocalCar();
		if(!car.IsNull())
		{
			Vector carLocation = car.GetLocation();
			Vector carLocationXY = Vector{carLocation.X, carLocation.Y, 0};
			float carHeightPerc = carLocation.Z / 1100;
			if(carHeightPerc > 0.9f)
				carHeightPerc = 0.9f;
			if(carHeightPerc < 0.1f)
				carHeightPerc = 0.1f;
			
			Vector2 lineStart;
			Vector2 lineEnd;
			
			canvas.SetColor(200, 255, 255, 255);

			//Fill innerPoints and outerPoints and draw octagons (listed clockwise)
			Vector circlePoints[8];
			circlePoints[0] = Vector{100.0f, 0.0f, 0.0f};
			circlePoints[1] = Vector{70.71f, 70.71f, 0.0f};
			circlePoints[2] = Vector{0.0f, 100.0f, 0.0f};
			circlePoints[3] = Vector{-70.71f, 70.71f, 0.0f};
			circlePoints[4] = Vector{-100.0f, 0.0f, 0.0f};
			circlePoints[5] = Vector{-70.71f, -70.71f, 0.0f};
			circlePoints[6] = Vector{0.0f, -100.0f, 0.0f};
			circlePoints[7] = Vector{70.71f, -70.71f, 0.0f};

			for(int i=0; i<8; i++)
			{
				if(i < 7)
				{
					//outer circle
					lineStart = canvas.Project(carLocationXY + circlePoints[i]);
					lineEnd = canvas.Project(carLocationXY + circlePoints[i+1]);
					canvas.DrawLine(lineStart, lineEnd);

					//inner circle
					lineStart = canvas.Project(carLocationXY + circlePoints[i] - (circlePoints[i] * carHeightPerc));
					lineEnd = canvas.Project(carLocationXY + circlePoints[i+1] - (circlePoints[i+1] * carHeightPerc));
					canvas.DrawLine(lineStart, lineEnd);
				}
				else if(i == 7)
				{
					//outer circle
					lineStart = canvas.Project(carLocationXY + circlePoints[i]);
					lineEnd = canvas.Project(carLocationXY + circlePoints[0]);
					canvas.DrawLine(lineStart, lineEnd);

					//inner circle
					lineStart = canvas.Project(carLocationXY + circlePoints[i] - (circlePoints[i] * carHeightPerc));
					lineEnd = canvas.Project(carLocationXY + circlePoints[0] - (circlePoints[0] * carHeightPerc));
					canvas.DrawLine(lineStart, lineEnd);
				}
			}

			//Draw vertical line from car to ground
			lineStart = canvas.Project(carLocation);
			lineEnd = canvas.Project(carLocationXY);
			canvas.DrawLine(lineStart, lineEnd);
		}
	}
}

ServerWrapper SideViewPlugin::GetCurrentGameState()
{
	if(gameWrapper->IsInReplay())
		return gameWrapper->GetGameEventAsReplay().memory_address;
	else if(gameWrapper->IsInOnlineGame())
		return gameWrapper->GetOnlineGame();
	else
		return gameWrapper->GetGameEventAsServer();
}







//LEAVE THESE UNCHANGED


void SideViewPlugin::onUnload(){}
void SideViewPlugin::Initialize()
{
	//Install parent plugin if it isn't already installed. Ensure parent plugin is loaded.
	if(!std::filesystem::exists(gameWrapper->GetBakkesModPath() / "plugins" / "CameraControl.dll"))
		cvarManager->executeCommand("bpm_install 71");
	cvarManager->executeCommand("plugin load CameraControl", false);

	//Hook events
	gameWrapper->HookEvent("Function ProjectX.Camera_X.ClampPOV", std::bind(&SideViewPlugin::HandleValues, this));
	gameWrapper->HookEvent("Function TAGame.PlayerController_TA.PressRearCamera", [&](std::string eventName){isInRearCam = true;});
	gameWrapper->HookEvent("Function TAGame.PlayerController_TA.ReleaseRearCamera", [&](std::string eventName){isInRearCam = false;});
	gameWrapper->HookEvent("Function TAGame.CameraState_BallCam_TA.BeginCameraState", [&](std::string eventName){isInBallCam = true;});
	gameWrapper->HookEvent("Function TAGame.CameraState_BallCam_TA.EndCameraState", [&](std::string eventName){isInBallCam = false;});
}
bool SideViewPlugin::CanCreateValues()
{
	if(!enabled || IsCVarNull("CamControl_Swivel_READONLY") || IsCVarNull("CamControl_Focus") || IsCVarNull("CamControl_Rotation") || IsCVarNull("CamControl_Distance") || IsCVarNull("CamControl_FOV"))
		return false;
	else
		return true;
}
bool SideViewPlugin::IsCVarNull(string cvarName)
{
    struct CastStructOne
    {
        struct CastStructTwo{void* address;};
        CastStructTwo* casttwo;
    };

	CVarWrapper cvar = cvarManager->getCvar(cvarName);
    CastStructOne* castone = (CastStructOne*)&cvar;
    return castone->casttwo->address == NULL;
}
void SideViewPlugin::Enable()
{
	cvarManager->executeCommand("CamControl_Enable 1", false);
	enabled = true;
	gameWrapper->RegisterDrawable(bind(&SideViewPlugin::Render, this, std::placeholders::_1));
}
void SideViewPlugin::Disable()
{
	gameWrapper->UnregisterDrawables();
	enabled = false;
	cvarManager->executeCommand("CamControl_Enable 0", false);
}
void SideViewPlugin::HandleValues()
{
	if(!CanCreateValues())
		return;
	
	//Reset values so that the game won't crash if the developer doesn't assign values to variables
	overrideValue[0] = true;//Focus X
	overrideValue[1] = true;//Focus Y
	overrideValue[2] = true;//Focus Z
	overrideValue[3] = true;//Rotation Pitch
	overrideValue[4] = true;//Rotation Yaw
	overrideValue[5] = true;//Rotation Roll
	overrideValue[6] = true;//Distance
	overrideValue[7] = true;//FOV

	SWIVEL = GetSwivel();
	FOCUS = Vector{0,0,0};
	ROTATION = Rotator{0,0,0};
	DISTANCE = 100;
	FOV = 90;

	//Get values from the developer
	CreateValues();

	//Send value requests to the parent mod
	string values[8];
	values[0] = to_string(FOCUS.X);
	values[1] = to_string(FOCUS.Y);
	values[2] = to_string(FOCUS.Z);
	values[3] = to_string(ROTATION.Pitch);
	values[4] = to_string(ROTATION.Yaw);
	values[5] = to_string(ROTATION.Roll);
	values[6] = to_string(DISTANCE);
	values[7] = to_string(FOV);
	
	for(int i=0; i<8; i++)
	{
		if(!overrideValue[i])
			values[i] = "NULL";
	}

	cvarManager->getCvar("CamControl_Focus").setValue(values[0] + "," + values[1] + "," + values[2]);
	cvarManager->getCvar("CamControl_Rotation").setValue(values[3] + "," + values[4] + "," + values[5]);
	cvarManager->getCvar("CamControl_Distance").setValue(values[6]);
	cvarManager->getCvar("CamControl_FOV").setValue(values[7]);
}
Rotator SideViewPlugin::GetSwivel()
{
	if(IsCVarNull("CamControl_Swivel_READONLY"))
		return Rotator{0,0,0};

	string readSwivel = cvarManager->getCvar("CamControl_Swivel_READONLY").getStringValue();
	string swivelInputString;
	stringstream ssSwivel(readSwivel);

	Rotator SWIVEL = {0,0,0};

	getline(ssSwivel, swivelInputString, ',');
	SWIVEL.Pitch = stof(swivelInputString);
	getline(ssSwivel, swivelInputString, ',');
	SWIVEL.Yaw = stof(swivelInputString);
	getline(ssSwivel, swivelInputString, ',');
	SWIVEL.Roll = stof(swivelInputString);

	return SWIVEL;
}