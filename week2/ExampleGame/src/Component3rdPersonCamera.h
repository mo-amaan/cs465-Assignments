#pragma once
#include "ComponentBase.h"
#include "SceneCamera.h"

class Component3rdPersonCamera : public Common::ComponentBase
{
public:
	Component3rdPersonCamera();
	virtual ~Component3rdPersonCamera();

	virtual const std::string FamilyID() { return std::string("GOC_3rdPersonCamera"); }
	virtual const std::string ComponentID() { return std::string("GOC_3rdPersonCamera"); }
	virtual void Update(float p_fDelta);
private:

	Common::SceneCamera* m_pCamera;

};
