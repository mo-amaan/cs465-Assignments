//========================================================================
// Week 11 - Lighting Part 2
//
// Example 1: Directional Light (but better structure than week 10!)
//========================================================================
#define GLFW_INCLUDE_GL3
#define GLFW_NO_GLU
#include <stdio.h>
#include <stdlib.h>
#ifndef __APPLE__
#include <GL/glew.h>
#endif
#include <GL/glfw.h>
#include "W_Common.h"
#include "W_Model.h" 

struct DirectionalLight
{
	glm::vec3		m_vDirection;
    wolf::Color4    m_ambient;
	wolf::Color4	m_diffuse;
	wolf::Color4	m_specular;
    DirectionalLight() : m_diffuse(0,0,0,0), m_specular(0,0,0,0), m_ambient(0,0,0,0) {}    
};

static DirectionalLight g_light;
static wolf::Model* g_pModel;

void InitExample1()
{
	g_light.m_diffuse = wolf::Color4(1.0f,1.0f,1.0f,1.0f);
	g_light.m_ambient = wolf::Color4(0.3f,0.3f,0.3f,1.0f);
	g_light.m_specular = wolf::Color4(1.0f,1.0f,1.0f,1.0f);
	g_light.m_vDirection = glm::vec3(0.0f,0.0f,-1.0f);

	g_pModel = new wolf::Model("data/animTest/zombie/zombie.pod", "data/animTest/zombie/", "data/animTest/skinned.vsh", "data/animTest/skinned.fsh");
	//g_pModel = new wolf::Model("data/animTest/zombie/zombie_gangnam.pod", "data/animTest/zombie/", "data/animTest/skinned.vsh", "data/animTest/skinned.fsh");
	//g_pModel = new wolf::Model("data/animTest/character/character.pod", "data/animTest/character/", "data/animTest/skinned.vsh", "data/animTest/skinned.fsh");
}

void RenderExample1(float p_fDeltaTime)
{
	// Update animations
	g_pModel->Update(p_fDeltaTime);

	// Set the light parameters
	g_pModel->GetMaterial()->SetUniform("ViewDir", glm::vec3(0.0f,0.0f,1.0f));
	g_pModel->GetMaterial()->SetUniform("LightAmbient", g_light.m_ambient);
	g_pModel->GetMaterial()->SetUniform("LightDiffuse", g_light.m_diffuse);
	g_pModel->GetMaterial()->SetUniform("LightSpecular", g_light.m_specular);
	g_pModel->GetMaterial()->SetUniform("LightDir", g_light.m_vDirection);

    static float s_fRot = 0.0f;
	glm::mat4 mProj = glm::perspective(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
	glm::mat4 mView = glm::lookAt(glm::vec3(0.0f, 120.0f, 300.0f), glm::vec3(0.0f,80.0f,0.0f), glm::vec3(0.0f,1.0f,0.0f));
	glm::mat4 mWorld = glm::rotate(s_fRot, 0.0f, 1.0f, 0.0f);
    
    if( glfwGetKey( GLFW_KEY_LEFT ) == GLFW_PRESS )
		s_fRot -= 1.0f;
	else if( glfwGetKey( GLFW_KEY_RIGHT ) == GLFW_PRESS )
		s_fRot += 1.0f;

	g_pModel->SetTransform(mWorld);
	g_pModel->Render(mView,mProj);
}


