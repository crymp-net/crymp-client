#include <string>
#include <vector>
#include <set>
#include <map>
#include <array>

struct IMaterial;
struct IRenderNode;

struct SAdMaterialMapping {
	float aspectRatio;
	int id;
};

struct SAdNodeInformation {
	IRenderNode* node;
	int materialId;
	bool parent;

	bool operator==(const SAdNodeInformation& rhs) const {
		return std::tie(node, materialId, parent) == std::tie(rhs.node, rhs.materialId, rhs.parent);
	}

	bool operator<(const SAdNodeInformation& rhs) const {
		return std::tie(node, materialId, parent) < std::tie(rhs.node, rhs.materialId, rhs.parent);
	}
};

struct SAdVersion {
	std::string id;
	std::string path;
	float aspectRatio;
	std::set<SAdNodeInformation> nodes;
};

struct SAdGroup {
	std::string id;
	std::string viewId;
	std::vector<SAdVersion> versions;
};

struct SAdVec3 {
	float x;
	float y;
	float z;
};

struct SAdCollectedRecord {
	size_t epoch;
	std::string groupId;
	std::string versionId;
	std::string viewId;

	std::string serverToken;
	std::string map;
	SAdVec3 cameraPos;
	SAdVec3 cameraDir;
	SAdVec3 pos;
};

enum class EAdState {
	eAS_None,

	eAS_FetchAds,
	eAS_FetchingAds,
	eAS_FetchedAds,

	eAS_FetchAdsPak,
	eAS_FetchingAdsPak,
	eAS_FetchedAdsPak,

	eAS_Display,
	eAS_Displaying,

	eAS_Error,
	eAS_DisplayError
};

class CAdManager {

	std::vector<SAdGroup> m_ads;
	std::vector<SAdGroup> m_upcomingAds;

	std::string m_pakPath;
	std::string m_upcomingPakPath;

	std::string m_pakUrl;
	std::string m_upcomingPakUrl;

	std::vector<SAdCollectedRecord> m_collectedRecords;

	std::map<std::string, SAdMaterialMapping> m_mappings;
	std::array<float, 4> m_cycles = { -1000.0f, -1000.0f, -1000.0f, -1000.0f };

	float m_idleTime = 0.0f;
	float m_time = 0.0f;
	bool m_collectedMappings = false;

	size_t m_epoch = 0;

	EAdState m_state = EAdState::eAS_None;

public:
	CAdManager();
	void Update(float delta);
	void Reset(bool quit = true);

private:
	void CollectViewership(float delta);
	void LoadAds(const std::string& response);
	void FetchAds();
	void FetchAdsPak();
	void DisplayAds();
	void CycleAds(float delta);

	std::map<std::string, SAdMaterialMapping>::iterator FindMaterialMapping(IRenderNode* node, const std::string& name);
	IMaterial* FindMaterial(const SAdGroup& group, const SAdVersion& version);
};