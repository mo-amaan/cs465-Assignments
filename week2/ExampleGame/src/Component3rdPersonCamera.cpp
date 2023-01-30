#include "Component3rdPersonCamera.h"
#include "GameObject.h"


Component3rdPersonCamera::Component3rdPersonCamera() {

}

Component3rdPersonCamera::~Component3rdPersonCamera() {

}

void Component3rdPersonCamera::Update(float p_fDelta) {

	Common::Transform& transform = this->GetGameObject()->GetTransform();
	auto& translation = transform.GetTranslation();
	auto& rotation = transform.GetRotation();

	//m_pCamera->SetPos(translation);

}