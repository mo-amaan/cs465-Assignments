#include "ComponentFloor.h"
#include "W_Model.h"
#include "GameObject.h"
#include "SceneManager.h"

using namespace week2;

//------------------------------------------------------------------------------
// Method:    ComponentRenderableMesh
// Returns:   
// 
// Constructor.
//------------------------------------------------------------------------------
ComponentFloor::ComponentFloor()
	:
	m_pSprite(NULL)
{
}

//------------------------------------------------------------------------------
// Method:    ~ComponentFloor
// Returns:   
// 
// Destructor.
//------------------------------------------------------------------------------
ComponentFloor::~ComponentFloor()
{
	// Remove the model from the scene
	Common::SceneManager::Instance()->RemoveModel(m_pSprite);

	// Delete the model
	if (m_pSprite)
	{
		delete m_pSprite;
		m_pSprite = NULL;
	}
}

//------------------------------------------------------------------------------
// Method:    Init
// Parameter: const std::string & p_strPath
// Parameter: const std::string & p_strTexturePath
// Parameter: const std::string & p_strVertexProgramPath
// Parameter: const std::string & p_strFragmentProgramPath
// Returns:   void
// 
// Initializes this component by loading the renderable object.
//------------------------------------------------------------------------------
void ComponentFloor::Init(const std::string& p_strTexture, const std::string& p_strVertexProgram, const std::string& p_strFragmentProgram, const glm::vec2& p_vDimensions)
{
	m_pSprite = new wolf::Sprite(p_strTexture,p_strVertexProgram,p_strFragmentProgram,p_vDimensions);
	Common::SceneManager::Instance()->AddSprite(m_pSprite);
}

//------------------------------------------------------------------------------
// Method:    SyncTransform
// Returns:   void
// 
// TODO: document me.
//------------------------------------------------------------------------------
void ComponentFloor::SyncTransform()
{
	m_pSprite->SetTransform(this->GetGameObject()->GetTransform().GetTransformation());
}