//Specification file for the TASEdit Project class

#include "movie.h"
#include "inputsnapshot.h"
#include "inputhistory.h"
#include "playback.h"
#include "greenzone.h"
#include "markers.h"

class TASEDIT_PROJECT
{
public:
	TASEDIT_PROJECT();
	void init();
	void reset();
	void update();

	bool saveProject();
	bool LoadProject(std::string PFN);

	bool Export2FM2(std::string filename);	//creates a fm2 out of header, comments, subtitles, and main branch input log, return false if any errors occur

	std::string GetProjectName();
	void SetProjectName(std::string e);
	std::string GetFM2Name();
	void SetFM2Name(std::string e);
	std::string GetProjectFile();
	void SetProjectFile(std::string e);

	// public vars
	bool changed, old_changed;

private:
	std::string projectName;			//The TASEdit Project's name
	std::string fm2FileName;			//The main branch ilog file (todo rename more appropriately)
	std::string projectFile; 			//The TASEdit Project's filename (For saving purposes) //adelikat: TODO: why the hell is this different from project name??!
	std::vector<std::string> inputlogs;	//List of associated .ilog files
	std::vector<std::string> comments;
	std::vector<std::string> subtitles;

};
