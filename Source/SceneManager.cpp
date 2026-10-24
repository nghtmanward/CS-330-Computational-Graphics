///////////////////////////////////////////////////////////////////////////////
// shadermanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
//
//  MODIFIED: Shawn Ward - CS-330 Milestone Four (texturing)
//  - LoadSceneTextures(): loads the scene's textures from Source/textures
//  - desk board: tiled wood texture (complex technique: tiling)
//  - French press: steel base band and lid, coffee inside, black plastic
//    knob and handle, and a see-through glass carafe layered over the
//    coffee (complex technique: overlapping textures with alpha blending)
//
//  MODIFIED: Shawn Ward - CS-330 Milestone Five (lighting and full scene)
//  - DefineObjectMaterials() / SetupSceneLights(): Phong materials and four
//    point lights modeled on the truck cab (dome, reading light, laptop
//    screen glow, rim light toward the curtain)
//  - full scene: curtain backdrop, desk board and C++ mouse mat, laptop,
//    pen, and mouse added alongside the French press, using textures made
//    from photos of the real workstation
//  - RenderScene() split into one render function per object
//
//  MODIFIED: Shawn Ward - CS-330 Final Project
//  - SetGroupTransformations(): turns a multi-part object as one unit
//  - laptop and pen turned 12 degrees to match the reference photo
//  - dome light made amber so the scene has a colored light
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class
 ***********************************************************/
SceneManager::SceneManager(ShaderManager* pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class
 ***********************************************************/
SceneManager::~SceneManager()
{
	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format - it supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			return false;
		}

		// generate the texture mipmaps for mapping textures to lower resolutions
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // Unbind the texture

		// register the loaded texture and associate it with the special tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// Error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding the loaded textures to
 *  OpenGL texture memory slots.  There are up to 16 slots.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the memory in all the
 *  used texture memory slots.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glGenTextures(1, &m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for setting the transform buffer
 *  using the passed in transformation values.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// set the scale value in the transform buffer
	scale = glm::scale(scaleXYZ);
	// set the rotation values in the transform buffer
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// set the translation value in the transform buffer
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, modelView);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for setting the texture data
 *  associated with the passed in ID into the shader.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, true);

		int textureID = -1;
		textureID = FindTextureSlot(textureTag);
		m_pShaderManager->setSampler2DValue(g_TextureValueName, textureID);
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/

// scene dimensions in world units, kept in one place so they can be
// updated from tape measurements of the real board and mat. scale
// reference: the laptop is 6 units wide, so 1 unit is about 2.4 inches
namespace
{
	const float BOARD_LENGTH = 20.0f;	// left to right
	const float BOARD_DEPTH = 5.6f;		// front to back
	const float MAT_LENGTH = 14.75f;	// matches the mat photo's ~2.95:1 shape
	const float MAT_DEPTH = 5.0f;
	const float SCENE_CENTER_Z = -1.0f;	// board and mat center under the laptop

	// curtain spans exactly the board's length and hangs just behind
	// the board's back edge, so its sides line up with the board's ends
	const float CURTAIN_WIDTH = BOARD_LENGTH;
	const float CURTAIN_HEIGHT = 14.0f;	// tall enough to fill the top of the view
	const float CURTAIN_Z = SCENE_CENTER_Z - (BOARD_DEPTH / 2.0f) - 0.5f;

	// laptop group: centered left to right on the board, turned 12
	// degrees toward the right like the reference photo; the pen
	// rides along so it stays on the palm rest
	const glm::vec3 LAPTOP_CENTER = glm::vec3(0.0f, 0.0f, -0.9f);
	const float LAPTOP_YAW = 12.0f;
}


/***********************************************************
 *  LoadSceneTextures()
 *
 *  This method loads every texture image used in the scene
 *  and registers each one with a tag. The images ship inside
 *  the Source/textures folder so the project is self-contained;
 *  paths are relative to the project folder, which Visual
 *  Studio uses as the working directory when running.
 ***********************************************************/
void SceneManager::LoadSceneTextures()
{
	bool bReturn = false;

	// OpenGL assumes each row of pixel data starts on a 4-byte boundary;
	// RGB photos whose width is not a multiple of 4 (the curtain is 1473
	// pixels wide) break that rule and make the driver read past the end
	// of the image, so rows are unpacked byte by byte instead
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// desk board: original photo by Shawn Ward of the actual board in his
	// truck cab, shot straight down, cropped to the board, and evened out
	// with Shadows/Highlights in Photopea to remove glare
	bReturn = CreateGLTexture("Source/textures/wood.jpg", "deskWood");

	// press frame: "Metal Silver Stainless Steel" by Karen Arnold
	// (PublicDomainPictures.net), via Needpix.com, Public Domain
	// (https://www.needpix.com/photo/1348787/metal-silver-stainlesssteel-brushedmetal-texture-wallpaper-background-art-abstract)
	bReturn = CreateGLTexture("Source/textures/metal.jpg", "metal");

	// carafe: original texture created by Shawn Ward in Photopea for this
	// project; a mostly transparent RGBA image with a faint glass tint and
	// soft vertical highlights, so the alpha channel lets the coffee inside
	// show through the glass
	bReturn = CreateGLTexture("Source/textures/glass.png", "glass");

	// coffee inside the carafe: original photo by Shawn Ward of brewed
	// black coffee, cropped square, brightened, and warmed in Photopea so
	// it still reads as coffee under the scene lighting
	bReturn = CreateGLTexture("Source/textures/coffee.jpg", "coffee");

	// knob and handle: "Black Plastic Texture" by ShadowRunner27, DeviantArt (2007),
	// licensed under CC BY-NC-ND 3.0 (https://creativecommons.org/licenses/by-nc-nd/3.0/)
	// source: https://www.deviantart.com/shadowrunner27/art/Black-Plastic-Texture-67944451
	// converted from grayscale to RGB only, as technically required for
	// CreateGLTexture() to load it; the image content is unchanged
	bReturn = CreateGLTexture("Source/textures/black_plastic.jpg", "plastic");

	// mouse mat: original photo by Shawn Ward of his C++ reference mat,
	// shot straight down, rotated level, and cropped to the stitched edge
	bReturn = CreateGLTexture("Source/textures/deskmat.jpg", "deskmat");

	// backdrop: original photo by Shawn Ward of the light-blocking curtain
	// in his truck cab, lit at an angle by the reading light to show folds
	bReturn = CreateGLTexture("Source/textures/curtain.jpg", "curtain");

	// laptop keyboard and display: original photos by Shawn Ward of his
	// own laptop; if either file is missing, RenderLaptop() falls back to
	// a flat color so the scene still renders
	bReturn = CreateGLTexture("Source/textures/keyboard.jpg", "keyboard");
	bReturn = CreateGLTexture("Source/textures/screen.jpg", "screen");

	// after the texture image data is loaded into memory, the
	// loaded textures need to be bound to texture slots - there
	// are a total of 16 available slots for scene textures
	BindGLTextures();
}

/***********************************************************
 *  DefineObjectMaterials()
 *
 *  Defines the Phong surface properties for each object.
 *  Higher shininess gives a tighter highlight; stronger
 *  specular color gives a brighter reflection. Ambient
 *  keeps every surface faintly visible so nothing in the
 *  scene falls into complete shadow.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	// builds one material and adds it to the scene's material list
	auto addMaterial = [this](
		std::string tag,
		float ambientStrength,
		glm::vec3 diffuseColor,
		glm::vec3 specularColor,
		float shininess)
		{
			OBJECT_MATERIAL material;
			material.ambientStrength = ambientStrength;
			material.ambientColor = diffuseColor;	// ambient takes on the surface's own tint
			material.diffuseColor = diffuseColor;
			material.specularColor = specularColor;
			material.shininess = shininess;
			material.tag = tag;
			m_objectMaterials.push_back(material);
		};

	// desk board: sealed wood, semi-gloss so the plane reflects the dome light
	addMaterial("wood", 0.2f, glm::vec3(0.5f, 0.45f, 0.4f), glm::vec3(0.35f, 0.32f, 0.3f), 24.0f);

	// mouse mat: printed cloth top with a soft sheen
	addMaterial("deskmat", 0.15f, glm::vec3(0.45f, 0.45f, 0.45f), glm::vec3(0.15f, 0.15f, 0.2f), 12.0f);

	// curtain: black faux leather; low diffuse keeps it reading as black
	// like the real curtain, while the specular keeps a soft sheen on the folds
	addMaterial("curtain", 0.15f, glm::vec3(0.3f, 0.3f, 0.3f), glm::vec3(0.3f, 0.3f, 0.3f), 20.0f);

	// brushed steel: press frame, pen, laptop deck; kept below full
	// strength because the light gray texture washes out to white
	// under the overhead dome light
	addMaterial("metal", 0.2f, glm::vec3(0.25f, 0.25f, 0.25f), glm::vec3(0.45f, 0.45f, 0.45f), 6.0f);

	// black plastic: knob, handle, laptop lid, mouse
	addMaterial("plastic", 0.3f, glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.25f, 0.25f, 0.25f), 12.0f);

	// laptop deck: matte metal-look plastic, so only a faint sheen;
	// the silver deck photo is light, and the four lights' diffuse
	// add together, so diffuse is kept low to hold the key and palm
	// rest detail instead of washing out to white
	addMaterial("laptopDeck", 0.2f, glm::vec3(0.3f, 0.3f, 0.3f), glm::vec3(0.08f, 0.08f, 0.08f), 4.0f);

	// glossy black mouse shell: dark, with small sharp highlights so the
	// lighting shows its curves like the real mouse. in this shader,
	// shininess scales highlight brightness (the light's focalStrength
	// sets how tight it is), so it is kept low to avoid blowing out
	addMaterial("glossyPlastic", 0.2f, glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0.35f, 0.35f, 0.35f), 6.0f);
	
	// coffee: liquid surface with a sharp, wet highlight
	addMaterial("coffee", 0.3f, glm::vec3(0.6f, 0.55f, 0.5f), glm::vec3(0.6f, 0.6f, 0.6f), 80.0f);

	// glass: very sharp highlight (the carafe itself is drawn unlit, see
	// RenderFrenchPress, so this mainly documents the intended surface)
	addMaterial("glass", 0.3f, glm::vec3(0.5f, 0.5f, 0.55f), glm::vec3(0.9f, 0.9f, 0.95f), 95.0f);

	// laptop display: moderate ambient so the screen reads as lit
	// without blowing out the photo's colors
	addMaterial("screen", 0.4f, glm::vec3(0.55f, 0.55f, 0.55f), glm::vec3(0.15f, 0.15f, 0.15f), 60.0f);
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  Four point lights modeled on the real truck cab:
 *  0 - amber overhead dome light (key light, colored)
 *  1 - reading light from the front left (fill light)
 *  2 - dim cool glow from the laptop screen onto the keyboard
 *  3 - dim rim light behind the objects, toward the curtain
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// enable lighting calculations in the fragment shader
	m_pShaderManager->setBoolValue(g_UseLightingName, true);

	// light 0: amber dome light overhead, the main light source
	// and the scene's colored light
	m_pShaderManager->setVec3Value("lightSources[0].position", 0.0f, 10.0f, -1.0f);
	m_pShaderManager->setVec3Value("lightSources[0].ambientColor", 0.12f, 0.07f, 0.02f);
	m_pShaderManager->setVec3Value("lightSources[0].diffuseColor", 0.6f, 0.42f, 0.2f);
	m_pShaderManager->setVec3Value("lightSources[0].specularColor", 0.6f, 0.45f, 0.25f);
	m_pShaderManager->setFloatValue("lightSources[0].focalStrength", 32.0f);
	m_pShaderManager->setFloatValue("lightSources[0].specularIntensity", 0.35f);

	// light 1: reading light from the front left, fills the shadow sides
	m_pShaderManager->setVec3Value("lightSources[1].position", -8.0f, 6.0f, 5.0f);
	m_pShaderManager->setVec3Value("lightSources[1].ambientColor", 0.05f, 0.05f, 0.05f);
	m_pShaderManager->setVec3Value("lightSources[1].diffuseColor", 0.25f, 0.24f, 0.22f);
	m_pShaderManager->setVec3Value("lightSources[1].specularColor", 0.3f, 0.3f, 0.3f);
	m_pShaderManager->setFloatValue("lightSources[1].focalStrength", 16.0f);
	m_pShaderManager->setFloatValue("lightSources[1].specularIntensity", 0.2f);

	// light 2: laptop screen glow, low and just in front of the display;
	// kept very dim because the course shader has no distance falloff
	m_pShaderManager->setVec3Value("lightSources[2].position", 0.0f, 1.6f, -2.4f);
	m_pShaderManager->setVec3Value("lightSources[2].ambientColor", 0.0f, 0.0f, 0.0f);
	m_pShaderManager->setVec3Value("lightSources[2].diffuseColor", 0.1f, 0.12f, 0.16f);
	m_pShaderManager->setVec3Value("lightSources[2].specularColor", 0.1f, 0.12f, 0.16f);
	m_pShaderManager->setFloatValue("lightSources[2].focalStrength", 8.0f);
	m_pShaderManager->setFloatValue("lightSources[2].specularIntensity", 0.1f);

	// light 3: rim light between the laptop and the curtain, separating
	// the objects from the dark backdrop and catching the curtain folds
	m_pShaderManager->setVec3Value("lightSources[3].position", 3.0f, 6.0f, -3.9f);
	m_pShaderManager->setVec3Value("lightSources[3].ambientColor", 0.02f, 0.02f, 0.02f);
	m_pShaderManager->setVec3Value("lightSources[3].diffuseColor", 0.25f, 0.25f, 0.25f);
	m_pShaderManager->setVec3Value("lightSources[3].specularColor", 0.35f, 0.35f, 0.35f);
	m_pShaderManager->setFloatValue("lightSources[3].focalStrength", 24.0f);
	m_pShaderManager->setFloatValue("lightSources[3].specularIntensity", 0.3f);
}

/***********************************************************
 *  SetTextureOrColor()
 *
 *  Uses the tagged texture when its image loaded; otherwise
 *  falls back to a flat color, so a missing image file
 *  never breaks the render.
 ***********************************************************/
void SceneManager::SetTextureOrColor(
	std::string textureTag,
	float redColorValue,
	float greenColorValue,
	float blueColorValue)
{
	if (FindTextureSlot(textureTag) >= 0)
	{
		SetShaderTexture(textureTag);
	}
	else
	{
		SetShaderColor(redColorValue, greenColorValue, blueColorValue, 1.0f);
	}
}

/***********************************************************
 *  SetGroupTransformations()
 *
 *  Builds a part's model matrix in two stages: first the
 *  part's own scale, rotation, and offset from the group's
 *  center (same order as SetTransformations), then one turn
 *  and move shared by the whole group. Parts keep their
 *  places relative to each other, so a multi-part object
 *  can be turned or moved by changing a single value.
 ***********************************************************/
void SceneManager::SetGroupTransformations(
	glm::vec3 groupPositionXYZ,
	float groupYawDegrees,
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 offsetXYZ)
{
	// the part's own transform, relative to the group center
	glm::mat4 local =
		glm::translate(offsetXYZ) *
		glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f)) *
		glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
		glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f)) *
		glm::scale(scaleXYZ);

	// the shared group transform: turn around the center, then place it
	glm::mat4 group =
		glm::translate(groupPositionXYZ) *
		glm::rotate(glm::radians(groupYawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, group * local);
	}
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by loading
 *  the shapes, textures in memory to support the 3D scene
 *  rendering
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// load the textures for the 3D scene
	LoadSceneTextures();

	// lighting is applied after the textures are loaded
	DefineObjectMaterials();
	SetupSceneLights();

	// alpha blending lets the glass carafe's transparent texture
	// show the coffee drawn behind it
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene

	m_basicMeshes->LoadPlaneMesh();
	m_basicMeshes->LoadCylinderMesh();
	m_basicMeshes->LoadSphereMesh();
	m_basicMeshes->LoadTorusMesh();
	m_basicMeshes->LoadBoxMesh();
	m_basicMeshes->LoadConeMesh();
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by
 *  transforming and drawing the basic 3D shapes
 ***********************************************************/
void SceneManager::RenderScene()
{
	// opaque objects first, back to front
	RenderBackdrop();
	RenderDesktopBoard();
	RenderLaptop();
	RenderPen();
	RenderMouse();

	// the French press is drawn last because its see-through glass
	// carafe must blend over everything already on screen
	RenderFrenchPress();
}

/***********************************************************
 *  RenderBackdrop()
 *
 *  The black light-blocking curtain behind the desk: a plane
 *  stood upright facing the camera.
 ***********************************************************/
void SceneManager::RenderBackdrop()
{
	glm::vec3 scaleXYZ;
	glm::vec3 positionXYZ;

	// plane mesh spans -1 to 1, so scale is half the curtain size;
	// rotating 90 degrees on X stands it up to face the camera
	scaleXYZ = glm::vec3(CURTAIN_WIDTH / 2.0f, 1.0f, CURTAIN_HEIGHT / 2.0f);

	// bottom edge sits level with the board's surface
	positionXYZ = glm::vec3(0.0f, CURTAIN_HEIGHT / 2.0f - 0.01f, CURTAIN_Z);

	SetTransformations(scaleXYZ, 90.0f, 0.0f, 0.0f, positionXYZ);

	// complex technique (tiling): the curtain photo repeats twice across
	// the wide backdrop so its folds keep their real-world size
	SetShaderTexture("curtain");
	SetTextureUVScale(2.0f, 1.0f);
	SetShaderMaterial("curtain");

	m_basicMeshes->DrawPlaneMesh();
}

/***********************************************************
 *  RenderDesktopBoard()
 *
 *  The wooden board laid across the cab's pull-out table and
 *  console, with the C++ mouse mat on top of it.
 ***********************************************************/
void SceneManager::RenderDesktopBoard()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************/
	/*** DESK BOARD (plane)                                          ***/
	/*** Base of the scene: the board laid across the pull-out table ***/
	/*** and console in the truck cab. Its top surface is at y = 0,  ***/
	/*** so every object's base sits at y = 0 to rest on it. The     ***/
	/*** plane mesh spans -1 to 1, so scale is half the board size.  ***/
	/******************************************************************/
	// board size comes from the scene dimensions at the top of this section
	scaleXYZ = glm::vec3(BOARD_LENGTH / 2.0f, 1.0f, BOARD_DEPTH / 2.0f);

	// lying flat, so no rotation is needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// lowered a hair below y = 0 so the mat can sit at y = 0 without
	// the two surfaces flickering against each other
	positionXYZ = glm::vec3(0.0f, -0.01f, SCENE_CENTER_Z);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// desk board - photo of the real board, mapped once across the plane
	SetShaderTexture("deskWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");

	m_basicMeshes->DrawPlaneMesh();
	/****************************************************************/
	/****************************************************************/

	// board thickness: a box under the plane gives the board a visible
	// front edge instead of a paper-thin surface. its top sits just
	// below the plane so the two never flicker against each other
	scaleXYZ = glm::vec3(BOARD_LENGTH, 0.6f, BOARD_DEPTH);
	positionXYZ = glm::vec3(0.0f, -0.32f, SCENE_CENTER_Z);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderTexture("deskWood");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("wood");
	m_basicMeshes->DrawBoxMesh();

	/******************************************************************/
	/*** MOUSE MAT (plane)                                           ***/
	/*** The C++ reference mat covering most of the board. Its top   ***/
	/*** is at y = 0, so the objects resting on it keep their y = 0  ***/
	/*** bases.                                                      ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(MAT_LENGTH / 2.0f, 1.0f, MAT_DEPTH / 2.0f);
	positionXYZ = glm::vec3(0.0f, 0.0f, SCENE_CENTER_Z);

	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);

	SetShaderTexture("deskmat");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("deskmat");

	m_basicMeshes->DrawPlaneMesh();
}

/***********************************************************
 *  RenderLaptop()
 *
 *  Laptop modeled from reference photos: a dark gray base
 *  that extends a little past the deck at the back, a hinge
 *  block sitting on top of that rear strip, and a nearly
 *  vertical lid resting on the hinge. The deck photo is a
 *  plane laid across the top of the base in front of the lid.
 ***********************************************************/
void SceneManager::RenderLaptop()
{
	glm::vec3 scaleXYZ;
	glm::vec3 offsetXYZ;

	// every laptop part maps its texture once
	SetTextureUVScale(1.0f, 1.0f);

	// laptop base: dark gray body like the real laptop's sides; its top
	// is covered by the deck photo, so only the edges show this color.
	// it runs 2.175 in front of and behind the laptop's center, extending
	// 0.35 behind the deck photo to make room for the hinge on top
	scaleXYZ = glm::vec3(6.0f, 0.35f, 4.35f);
	offsetXYZ = glm::vec3(0.0f, 0.175f, 0.0f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 0.0f, 0.0f, offsetXYZ);
	SetShaderColor(0.3f, 0.3f, 0.32f, 1.0f);
	SetShaderMaterial("plastic");
	m_basicMeshes->DrawBoxMesh();

	// hinge block: dark bar sitting ON TOP of the base's rear strip,
	// behind the speaker grille, like the real laptop
	scaleXYZ = glm::vec3(5.0f, 0.15f, 0.3f);
	offsetXYZ = glm::vec3(0.0f, 0.425f, -2.025f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 0.0f, 0.0f, offsetXYZ);
	SetShaderTexture("plastic");
	SetShaderMaterial("plastic");
	m_basicMeshes->DrawBoxMesh();

	// lid housing: tilted back 8 degrees, bottom edge resting in the
	// hinge block at y = 0.4 (center = bottom edge + half the lid
	// height along the tilted direction)
	scaleXYZ = glm::vec3(6.0f, 4.0f, 0.2f);
	offsetXYZ = glm::vec3(0.0f, 2.38f, -2.305f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, -8.0f, 0.0f, 0.0f, offsetXYZ);
	SetShaderTexture("plastic");
	SetShaderMaterial("plastic");
	m_basicMeshes->DrawBoxMesh();

	// display panel: raised on the lid face to leave a
	// thick bottom bezel like the real screen
	scaleXYZ = glm::vec3(5.7f, 3.3f, 0.02f);
	offsetXYZ = glm::vec3(0.0f, 2.69f, -2.235f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, -8.0f, 0.0f, 0.0f, offsetXYZ);
	SetTextureOrColor("screen", 0.15f, 0.2f, 0.3f);
	SetShaderMaterial("screen");
	m_basicMeshes->DrawBoxMesh();

	// deck surface: a plane laid just above the base's top face, skinned
	// with a straightened top-down photo of the real deck (speaker
	// grille, keyboard, and touchpad all in their real spots). a plane
	// maps the whole image across its face, where the box mesh did not;
	// the plane mesh spans -1 to 1, so scale is half the deck size
	scaleXYZ = glm::vec3(3.0f, 1.0f, 2.0f);
	offsetXYZ = glm::vec3(0.0f, 0.355f, 0.175f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 0.0f, 0.0f, offsetXYZ);
	SetTextureOrColor("keyboard", 0.55f, 0.55f, 0.55f);
	SetShaderMaterial("laptopDeck");
	m_basicMeshes->DrawPlaneMesh();
}

/***********************************************************
 *  RenderPen()
 *
 *  Blue pen resting on the laptop's palm rest, right of the
 *  touchpad, as in the reference photo: a cylinder body, a
 *  chrome band near the cap end, and a chrome cone tip. All
 *  parts share one rotation: Z -90 lays each piece along +X,
 *  then Y 20 angles the tip toward the back right. The pen
 *  shares the laptop's group turn so it stays on the palm rest.
 ***********************************************************/
void SceneManager::RenderPen()
{
	glm::vec3 scaleXYZ;
	glm::vec3 offsetXYZ;

	SetTextureUVScale(1.0f, 1.0f);

	// the pen is placed relative to the laptop's center and shares its
	// group turn, so it stays in the same spot on the palm rest

	// pen body: starts at the cap end and runs 2.0 units along the
	// pen's direction (0.94, 0, -0.34); its axis sits at the deck
	// height (0.355) plus the pen's radius so it rests on the deck
	scaleXYZ = glm::vec3(0.08f, 2.0f, 0.08f);
	offsetXYZ = glm::vec3(0.7f, 0.435f, 1.925f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 20.0f, -90.0f, offsetXYZ);
	SetShaderColor(0.15f, 0.25f, 0.75f, 1.0f);
	SetShaderMaterial("glossyPlastic");
	m_basicMeshes->DrawCylinderMesh();

	// chrome band: short, slightly wider ring 0.35 units from the cap end
	scaleXYZ = glm::vec3(0.088f, 0.12f, 0.088f);
	offsetXYZ = glm::vec3(1.029f, 0.435f, 1.805f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 20.0f, -90.0f, offsetXYZ);
	SetShaderTexture("metal");
	SetShaderMaterial("metal");
	m_basicMeshes->DrawCylinderMesh();

	// pen tip: chrome cone at the far end of the body
	// (start point + 2.0 units along the pen's direction)
	scaleXYZ = glm::vec3(0.08f, 0.3f, 0.08f);
	offsetXYZ = glm::vec3(2.579f, 0.435f, 1.241f);
	SetGroupTransformations(LAPTOP_CENTER, LAPTOP_YAW, scaleXYZ, 0.0f, 20.0f, -90.0f, offsetXYZ);
	SetShaderTexture("metal");
	SetShaderMaterial("metal");
	m_basicMeshes->DrawConeMesh();
}

/***********************************************************
 *  RenderMouse()
 *
 *  Mouse modeled from reference photos of the real one: a
 *  glossy black shell tilted so its hump sits toward the
 *  back, a slightly wider matte base skirt underneath that
 *  forms the seam line around the bottom, and a scroll
 *  wheel near the front.
 ***********************************************************/
void SceneManager::RenderMouse()
{
	glm::vec3 scaleXYZ;
	glm::vec3 positionXYZ;

	SetTextureUVScale(1.0f, 1.0f);

	// base skirt: low, slightly wider oval under the shell; only its
	// edge shows, as the matte band around the bottom of the mouse
	scaleXYZ = glm::vec3(0.62f, 0.12f, 1.02f);
	positionXYZ = glm::vec3(4.6f, 0.0f, 0.7f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.1f, 0.1f, 0.1f, 1.0f);
	SetShaderMaterial("plastic");
	m_basicMeshes->DrawSphereMesh();

	// shell: oval dome raised onto the skirt and tilted back 8 degrees
	// so the front dips low and the hump rises toward the palm end
	scaleXYZ = glm::vec3(0.6f, 0.55f, 1.0f);
	positionXYZ = glm::vec3(4.6f, 0.12f, 0.7f);
	SetTransformations(scaleXYZ, -8.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderColor(0.15f, 0.15f, 0.16f, 1.0f);
	SetShaderMaterial("glossyPlastic");
	m_basicMeshes->DrawSphereMesh();

	// scroll wheel: short cylinder laid across the mouse,
	// set into the shell near the front
	scaleXYZ = glm::vec3(0.16f, 0.24f, 0.26f);
	positionXYZ = glm::vec3(4.72f, 0.38f, 0.15f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 90.0f, positionXYZ);
	SetShaderColor(0.22f, 0.22f, 0.22f, 1.0f);
	SetShaderMaterial("plastic");
	m_basicMeshes->DrawCylinderMesh();
}

/***********************************************************
 *  RenderFrenchPress()
 *
 *  The French press: steel bands and lid, coffee inside, a
 *  black plastic knob and handle, and the glass carafe
 *  drawn last so it blends over the coffee.
 ***********************************************************/
void SceneManager::RenderFrenchPress()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	/******************************************************************/
	/*** FRENCH PRESS - BASE BAND (cylinder)                         ***/
	/*** Steel ring around the bottom of the carafe, slightly wider  ***/
	/*** than the glass (0.9) so it wraps around the outside. It is  ***/
	/*** 0.55 tall (y = 0 to 0.55), so it also covers where the      ***/
	/*** bottom of the handle joins the carafe.                      ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(0.95f, 0.55f, 0.95f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// same base position as the carafe, resting on the plane
	positionXYZ = glm::vec3(5.0f, 0.0f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// brushed steel frame
	SetShaderTexture("metal");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("metal");

	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************/
	/*** FRENCH PRESS - COFFEE (cylinder)                            ***/
	/*** Liquid inside the carafe: narrower than the glass so it     ***/
	/*** sits inside it, starting on top of the base band and filled ***/
	/*** up to y = 2.30, where it meets the handle band so no part   ***/
	/*** of the handle shows through the glass above the coffee.     ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(0.85f, 2.15f, 0.85f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// starts at y = 0.4, hidden inside the base band (which runs to y = 0.55)
	positionXYZ = glm::vec3(5.0f, 0.4f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// brewed coffee
	SetShaderTexture("coffee");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("coffee");

	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************/
	/*** FRENCH PRESS - LID (cylinder)                               ***/
	/*** Steel band capping the top of the carafe, from y = 2.85 to  ***/
	/*** the carafe top at y = 3.0, where the plunger knob sits.     ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(0.95f, 0.15f, 0.95f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	positionXYZ = glm::vec3(5.0f, 2.85f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// brushed steel frame
	SetShaderTexture("metal");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("metal");

	m_basicMeshes->DrawCylinderMesh();


	/******************************************************************/
	/*** FRENCH PRESS - HANDLE BAND (cylinder)                       ***/
	/*** Steel band where the top of the handle joins the carafe.    ***/
	/*** The handle passes through the glass wall at about y = 2.49, ***/
	/*** so this band runs from y = 2.05 to 2.58 (0.53 tall) to      ***/
	/*** cover the join, like the frame on a real press.             ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(0.95f, 0.53f, 0.95f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	positionXYZ = glm::vec3(5.0f, 2.05f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// brushed steel frame
	SetShaderTexture("metal");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("metal");

	// sides only (no top or bottom cap), so the band is an open ring
	// around the glass instead of a solid disk showing through it
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	/******************************************************************/
	/*** FRENCH PRESS - PLUNGER KNOB (sphere)                        ***/
	/*** Sits centered on top of the carafe. Sphere mesh is centered ***/
	/*** on its own origin, so position Y = carafe height + radius.  ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(0.35f, 0.35f, 0.35f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// carafe top is at y = 3.0 (base 0.0 + height 3.0); lift knob by its radius
	positionXYZ = glm::vec3(5.0f, 3.35f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// black plastic knob
	SetShaderTexture("plastic");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("plastic");

	m_basicMeshes->DrawSphereMesh();

	// plunger rod: thin steel cylinder running down the center of the
	// carafe from the base band (y = 0.55) up into the knob (y = 3.35),
	// visible through the glass above the coffee
	scaleXYZ = glm::vec3(0.04f, 2.8f, 0.04f);
	positionXYZ = glm::vec3(5.0f, 0.55f, -1.5f);
	SetTransformations(scaleXYZ, 0.0f, 0.0f, 0.0f, positionXYZ);
	SetShaderTexture("metal");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("metal");
	m_basicMeshes->DrawCylinderMesh();

	/******************************************************************/
	/*** FRENCH PRESS - HANDLE (torus)                               ***/
	/*** Torus loads lying flat (hole facing Y). Rotating 90 deg on  ***/
	/*** X stands the ring up so it reads as a side handle. If your  ***/
	/*** ShapeMeshes orientation differs, try ZrotationDegrees = 90  ***/
	/*** instead and re-check against the reference photo.           ***/
	/******************************************************************/
	scaleXYZ = glm::vec3(1.0f, 0.45f, 0.45f);

	XrotationDegrees = 0.0f;
	YrotationDegrees = -15.0f;
	ZrotationDegrees = 90.0f;

	// tucked against the carafe's side, roughly mid-height
	positionXYZ = glm::vec3(5.92f, 1.3f, -1.22f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// dark metal frame/handle
	// textured as black plastic to match the knob
	SetShaderTexture("plastic");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("plastic");

	m_basicMeshes->DrawTorusMesh();

	// the see-through glass carafe is drawn LAST so every opaque part
	// behind and inside it is already on screen for blending to show
	// through the glass
	/******************************************************************/
	/*** FRENCH PRESS - CARAFE (cylinder)                            ***/
	/*** Main body of the press. Cylinder mesh sits base-up from     ***/
	/*** y = 0, so its position Y is the base height, not the center.***/
	/******************************************************************/
	// set the XYZ scale for the mesh (radius in X/Z, height in Y)
	scaleXYZ = glm::vec3(0.9f, 3.0f, 0.9f);

	// set the XYZ rotation for the mesh - upright, no rotation needed
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// set the XYZ position for the mesh - base resting on the plane
	positionXYZ = glm::vec3(5.0f, 0.0f, -1.5f);

	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// glass carafe - light gray
	// complex technique (overlapping textures): the glass texture's alpha
	// channel blends it over the coffee texture already drawn inside, so
	// two images layer on the same spot. this works because the course
	// shader passes texture alpha through when lighting is off; its
	// lighting path forces alpha to 1.0, so this will need revisiting
	// when lighting is added
	// revisited for Milestone Five: lighting is switched off for this one
	// draw so the glass keeps its transparency, then switched back on
	SetShaderTexture("glass");
	SetTextureUVScale(1.0f, 1.0f);
	SetShaderMaterial("glass");

	m_pShaderManager->setBoolValue(g_UseLightingName, false);
	m_basicMeshes->DrawCylinderMesh();
	m_pShaderManager->setBoolValue(g_UseLightingName, true);
	/****************************************************************/
}