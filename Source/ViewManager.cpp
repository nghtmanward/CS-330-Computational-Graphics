///////////////////////////////////////////////////////////////////////////////
// viewmanager.h
// ============
// manage the viewing of 3D objects within the viewport
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>    

// declaration of the global variables and defines
namespace
{
	// Variables for window width and height
	const int WINDOW_WIDTH = 1000;
	const int WINDOW_HEIGHT = 800;
	const char* g_ViewName = "view";
	const char* g_ProjectionName = "projection";

	// camera object used for viewing and interacting with
	// the 3D scene
	Camera* g_pCamera = nullptr;

	// these variables are used for mouse movement processing
	float gLastX = WINDOW_WIDTH / 2.0f;
	float gLastY = WINDOW_HEIGHT / 2.0f;
	bool gFirstMouse = true;

	// time between current frame and last frame
	float gDeltaTime = 0.0f; 
	float gLastFrame = 0.0f;

	// the following variable is false when orthographic projection
	// is off and true when it is on
	bool bOrthographicProjection = false;

	// clipping distances shared by both projection types
	const float NEAR_PLANE = 0.1f;
	const float FAR_PLANE = 100.0f;

	// default perspective camera placement: above and in front of
	// the desk, pitched down about 14 degrees to look at the scene
	const glm::vec3 DEFAULT_CAMERA_POSITION = glm::vec3(0.0f, 5.0f, 12.0f);
	const float DEFAULT_CAMERA_YAW = -90.0f;     // -90 faces down the -Z axis
	const float DEFAULT_CAMERA_PITCH = -14.0f;

	// half the height of the visible orthographic box, in world units;
	// the width is derived from the window aspect ratio. 8.5 fits the
	// full 20-unit board across the window
	const float ORTHO_HALF_HEIGHT = 8.5f;

	/***********************************************************
	 *  SetCameraAngles()
	 *
	 *  Sets the camera's yaw and pitch, then rebuilds its Front,
	 *  Right, and Up vectors. camera.h keeps updateCameraVectors()
	 *  private, so a zero-offset ProcessMouseMovement() call is
	 *  used to trigger that rebuild.
	 ***********************************************************/
	void SetCameraAngles(float yaw, float pitch)
	{
		g_pCamera->Yaw = yaw;
		g_pCamera->Pitch = pitch;
		g_pCamera->ProcessMouseMovement(0.0f, 0.0f);
	}


}

/***********************************************************
 *  ViewManager()
 *
 *  The constructor for the class
 ***********************************************************/
ViewManager::ViewManager(
	ShaderManager *pShaderManager)
{
	// initialize the member variables
	m_pShaderManager = pShaderManager;
	m_pWindow = NULL;
	g_pCamera = new Camera();
	
	// default camera view parameters.The view direction is set
	// through yaw/pitch (not Front directly) so the first mouse
	// movement does not snap the camera to a different angle.
	g_pCamera->Position = DEFAULT_CAMERA_POSITION;
	g_pCamera->WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	SetCameraAngles(DEFAULT_CAMERA_YAW, DEFAULT_CAMERA_PITCH);
	g_pCamera->Zoom = 80;

	// camera.h defaults to 0.01 degrees per pixel, which makes
	// mouse look feel stuck; 0.1 gives a usable turning rate
	g_pCamera->MouseSensitivity = 0.1f;
}

/***********************************************************
 *  ~ViewManager()
 *
 *  The destructor for the class
 ***********************************************************/
ViewManager::~ViewManager()
{
	// free up allocated memory
	m_pShaderManager = NULL;
	m_pWindow = NULL;
	if (NULL != g_pCamera)
	{
		delete g_pCamera;
		g_pCamera = NULL;
	}
}

/***********************************************************
 *  CreateDisplayWindow()
 *
 *  This method is used to create the main display window.
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
	GLFWwindow* window = nullptr;

	// try to create the displayed OpenGL window
	window = glfwCreateWindow(
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		windowTitle,
		NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return NULL;
	}
	glfwMakeContextCurrent(window);

	// tell GLFW to capture all mouse events; the cursor is hidden
	// and locked to the window so mouse look can turn freely
	// (press ESC to close the window)
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// this callback is used to receive mouse moving events
	glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);

	// this callback is used to receive mouse scroll wheel events
	glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Wheel_Callback);

	// enable blending for supporting tranparent rendering
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_pWindow = window;

	return(window);
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the mouse is moved within the active GLFW display window.
 *  It turns the camera left/right (yaw) and up/down (pitch).
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos)
{
	if (NULL == g_pCamera)
	{
		return;
	}

	// on the first mouse event, record the position so the offsets
	// below start from zero instead of jumping the camera
	if (gFirstMouse)
	{
		gLastX = (float)xMousePos;
		gLastY = (float)yMousePos;
		gFirstMouse = false;
	}

	// calculate how far the mouse moved since the last event
	float xOffset = (float)xMousePos - gLastX;
	float yOffset = gLastY - (float)yMousePos; // reversed: screen Y grows downward

	// store the current position for the next event
	gLastX = (float)xMousePos;
	gLastY = (float)yMousePos;

	// orthographic view stays locked at the orientation it had when
	// switched, so the flat view holds steady while the mouse moves
	if (bOrthographicProjection)
	{
		return;
	}

	// rotate the 3D camera according to the calculated offsets
	g_pCamera->ProcessMouseMovement(xOffset, yOffset);
}

/***********************************************************
 *  Mouse_Scroll_Wheel_Callback()
 *
 *  This method is automatically called from GLFW whenever
 *  the mouse scroll wheel is moved within the active GLFW
 *  display window. camera.h's ProcessMouseScroll() adjusts
 *  MovementSpeed (not field of view) by subtracting the
 *  offset, so the offset is negated here: scroll up = faster,
 *  scroll down = slower.
 ***********************************************************/
void ViewManager::Mouse_Scroll_Wheel_Callback(GLFWwindow* window, double xOffset, double yOffset)
{
	if (NULL == g_pCamera)
	{
		return;
	}

	g_pCamera->ProcessMouseScroll(-(float)yOffset);
}

/***********************************************************
 *  SwitchToOrthographic()
 *
 *  Switches to the orthographic (2D) projection matrix. The
 *  camera's position and orientation are left unchanged, so
 *  the view flattens in place.
 ***********************************************************/
void ViewManager::SwitchToOrthographic()
{
	// projection change only; camera position and orientation stay put
	bOrthographicProjection = true;
}

/***********************************************************
 *  SwitchToPerspective()
 *
 *  Switches back to the perspective (3D) projection matrix
 *  from the same camera position and orientation.
 ***********************************************************/
void ViewManager::SwitchToPerspective()
{
	bOrthographicProjection = false;
}

/***********************************************************
 *  ProcessKeyboardEvents()
 *
 *  This method is called to process any keyboard events
 *  that may be waiting in the event queue.
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
	// close the window if the escape key has been pressed
	if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(m_pWindow, true);
	}

	// if the camera object is null, then exit this method
	if (NULL == g_pCamera)
	{
		return;
	}

	// W / S: move forward and backward along the view direction
	if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(FORWARD, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(BACKWARD, gDeltaTime);
	}

	// A / D: strafe left and right
	if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(LEFT, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(RIGHT, gDeltaTime);
	}

	// Q / E: move straight up and down
	if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(UP, gDeltaTime);
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
	{
		g_pCamera->ProcessKeyboard(DOWN, gDeltaTime);
	}

	// O / P: switch projection. The mode checks make each key act
	// only once per switch while it is held down.
	if (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS && !bOrthographicProjection)
	{
		SwitchToOrthographic();
	}
	if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS && bOrthographicProjection)
	{
		SwitchToPerspective();
	}
}

/***********************************************************
 *  PrepareSceneView()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
	glm::mat4 view;
	glm::mat4 projection;

	// per-frame timing
	float currentFrame = (float)glfwGetTime();
	gDeltaTime = currentFrame - gLastFrame;
	gLastFrame = currentFrame;

	// process any keyboard events that may be waiting in the
	// event queue
	ProcessKeyboardEvents();

	// get the current view matrix from the camera
	view = g_pCamera->GetViewMatrix();

	// window aspect ratio, used by both projection types
	float aspectRatio = (GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT;

	// define the current projection matrix
	if (bOrthographicProjection)
	{
		// orthographic (2D): parallel projection with no perspective
		// foreshortening; the box width follows the aspect ratio so
		// objects are not stretched
		float halfWidth = ORTHO_HALF_HEIGHT * aspectRatio;
		projection = glm::ortho(-halfWidth, halfWidth,
			-ORTHO_HALF_HEIGHT, ORTHO_HALF_HEIGHT,
			NEAR_PLANE, FAR_PLANE);
	}
	else
	{
		// perspective (3D): distant objects appear smaller
		projection = glm::perspective(glm::radians(g_pCamera->Zoom), aspectRatio, NEAR_PLANE, FAR_PLANE);
	}

	// if the shader manager object is valid
	if (NULL != m_pShaderManager)
	{
		// set the view matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ViewName, view);
		// set the projection matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ProjectionName, projection);
		// set the view position of the camera into the shader for proper rendering
		m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
	}
}
