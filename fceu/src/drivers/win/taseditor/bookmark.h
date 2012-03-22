// Specification file for Bookmark class

#define FLASH_PHASE_MAX 11
enum
{
	FLASH_TYPE_SET = 0,
	FLASH_TYPE_JUMP = 1,
	FLASH_TYPE_DEPLOY = 2,
};

#define SCREENSHOT_WIDTH 256
#define SCREENSHOT_HEIGHT 240
#define SCREENSHOT_SIZE SCREENSHOT_WIDTH * SCREENSHOT_HEIGHT

class BOOKMARK
{
public:
	BOOKMARK();
	void init();

	void set();
	void jumped();
	void deployed();

	void save(EMUFILE *os);
	bool load(EMUFILE *is);

	// saved vars
	bool not_empty;
	SNAPSHOT snapshot;
	std::vector<uint8> savestate;
	std::vector<uint8> saved_screenshot;

	//int parent_branch;

	// not saved vars
	int flash_phase;
	int flash_type;

private:

};
