/* ---------------------------------------------------------------------------------
Implementation file of TASEDITOR_LUA class
Copyright (c) 2011-2012 AnS

(The MIT License)
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------------
Lua - Manager of Lua features
[Singleton]

* implements logic of all functions of "taseditor" Lua library
* stores the list of pending Input changes
* on demend: (from FCEUX Lua engine) updates GUI items on "Lua" panel of TAS Editor window
* stores resources: ids of joypads for Input changes, max length of a name for applychanges()
------------------------------------------------------------------------------------ */

#include "taseditor_project.h"

extern TASEDITOR_CONFIG taseditor_config;
extern TASEDITOR_WINDOW taseditor_window;
extern HISTORY history;
extern MARKERS_MANAGER markers_manager;
extern BOOKMARKS bookmarks;
extern BRANCHES branches;
extern RECORDER recorder;
extern PLAYBACK playback;
extern GREENZONE greenzone;
extern PIANO_ROLL piano_roll;
extern SELECTION selection;

extern void TaseditorUpdateManualFunctionStatus();

TASEDITOR_LUA::TASEDITOR_LUA()
{
}

void TASEDITOR_LUA::init()
{
	pending_changes.resize(0);
	hwndRunFunction = GetDlgItem(taseditor_window.hwndTasEditor, TASEDITOR_RUN_MANUAL);
	reset();
}
void TASEDITOR_LUA::reset()
{
	TaseditorUpdateManualFunctionStatus();
}
void TASEDITOR_LUA::update()
{

}

void TASEDITOR_LUA::EnableRunFunction()
{
	EnableWindow(hwndRunFunction, true);
}
void TASEDITOR_LUA::DisableRunFunction()
{
	EnableWindow(hwndRunFunction, false);
}

void TASEDITOR_LUA::InsertDelete_rows_to_Snaphot(SNAPSHOT& snapshot)
{
	int size = pending_changes.size();
	if (size)
	{
		// apply changes to given snapshot (only insertion/deletion)
		for (int i = 0; i < size; ++i)
		{
			if (pending_changes[i].frame >= snapshot.size)
				// expand snapshot to fit the frame
				snapshot.insertFrames(-1, 1 + pending_changes[i].frame - snapshot.size);
			switch (pending_changes[i].type)
			{
				case LUA_CHANGE_TYPE_INSERTFRAMES:
				{
					snapshot.insertFrames(pending_changes[i].frame, pending_changes[i].data);
					break;
				}
				case LUA_CHANGE_TYPE_DELETEFRAMES:
				{
					for (int t = pending_changes[i].data; t > 0; t--)
					{
						if (pending_changes[i].frame < snapshot.size)
							snapshot.eraseFrame(pending_changes[i].frame);
					}
					break;
				}
			}
		}
	}
}

// --------------------------------------------------------------------------------
// Lua functions of taseditor library

// bool taseditor.engaged()
bool TASEDITOR_LUA::engaged()
{
	return FCEUMOV_Mode(MOVIEMODE_TASEDITOR);
}

// bool taseditor.markedframe(int frame)
bool TASEDITOR_LUA::markedframe(int frame)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return markers_manager.GetMarker(frame) != 0;
	else
		return false;
}

// int taseditor.getmarker(int frame)
int TASEDITOR_LUA::getmarker(int frame)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return markers_manager.GetMarkerUp(frame);
	else
		return -1;
}

// int taseditor.setmarker(int frame)
int TASEDITOR_LUA::setmarker(int frame)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		int marker_id = markers_manager.GetMarker(frame);
		if (!marker_id)
		{
			marker_id = markers_manager.SetMarker(frame);
			if (marker_id)
			{
				// new Marker was created - register changes in TAS Editor
				history.RegisterMarkersChange(MODTYPE_LUA_MARKER_SET, frame);
				selection.must_find_current_marker = playback.must_find_current_marker = true;
				piano_roll.RedrawRow(frame);
				piano_roll.SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
			}
		}
		return marker_id;
	} else
		return -1;
}

// taseditor.removemarker(int frame)
void TASEDITOR_LUA::removemarker(int frame)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		if (markers_manager.GetMarker(frame))
		{
			markers_manager.ClearMarker(frame);
			// Marker was deleted - register changes in TAS Editor
			history.RegisterMarkersChange(MODTYPE_LUA_MARKER_REMOVE, frame);
			selection.must_find_current_marker = playback.must_find_current_marker = true;
			piano_roll.RedrawRow(frame);
			piano_roll.SetHeaderColumnLight(COLUMN_FRAMENUM, HEADER_LIGHT_MAX);
		}
	}
}

// string taseditor.getnote(int index)
const char* TASEDITOR_LUA::getnote(int index)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		return strdup(markers_manager.GetNote(index).c_str());
	} else
		return NULL;
}

// taseditor.setnote(int index, string newtext)
void TASEDITOR_LUA::setnote(int index, const char* newtext)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		// rename only if newtext is different from old text
		char text[MAX_NOTE_LEN];
		strncpy(text, newtext, MAX_NOTE_LEN - 1);
		if (strcmp(markers_manager.GetNote(index).c_str(), text))
		{
			// text differs from old Note - rename
			markers_manager.SetNote(index, text);
			history.RegisterMarkersChange(MODTYPE_LUA_MARKER_RENAME, markers_manager.GetMarkerFrame(index), -1, text);
			selection.must_find_current_marker = playback.must_find_current_marker = true;
		}
	}
}

// int taseditor.getcurrentbranch()
int TASEDITOR_LUA::getcurrentbranch()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return branches.GetCurrentBranch();
	else
		return -1;
}

// string taseditor.getrecordermode()
const char* TASEDITOR_LUA::getrecordermode()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return recorder.GetRecordingMode();
	else
		return NULL;
}

// int taseditor.getsuperimpose()
int TASEDITOR_LUA::getsuperimpose()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return taseditor_config.superimpose;
	else
		return -1;
}

// int taseditor.getlostplayback()
int TASEDITOR_LUA::getlostplayback()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return playback.GetLostPosition();
	else
		return -1;
}

// int taseditor.getplaybacktarget()
int TASEDITOR_LUA::getplaybacktarget()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		return playback.GetPauseFrame() - 1;
	else
		return -1;
}

// taseditor.setplayback(int frame)
void TASEDITOR_LUA::setplayback(int frame)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		playback.jump(frame, false, true);
}

// taseditor.stopseeking()
void TASEDITOR_LUA::stopseeking()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
		playback.SeekingStop();
}

// table taseditor.getselection()
void TASEDITOR_LUA::getselection(std::vector<int>& placeholder)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		SelectionFrames* current_selection = selection.MakeStrobe();
		int frames = current_selection->size();
		if (!frames) return;

		placeholder.resize(frames);
		SelectionFrames::iterator current_selection_end(current_selection->end());
		int i = 0;
		for(SelectionFrames::iterator it(current_selection->begin()); it != current_selection_end; ++it)
			placeholder[i++] = *it;
	}
}

// taseditor.setselection(table new_set)
void TASEDITOR_LUA::setselection(std::vector<int>& new_set)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		selection.ClearSelection();
		for (int i = new_set.size() - 1; i >= 0; i--)
			selection.SetRowSelection(new_set[i]);
	}
}

// int taseditor.getinput(int frame, int joypad)
int TASEDITOR_LUA::getinput(int frame, int joypad)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		if (frame < 0) return -1;
		if (frame >= currMovieData.getNumRecords()) return 0;
		switch (joypad)
		{
			case LUA_JOYPAD_COMMANDS:
				return currMovieData.records[frame].commands;
			case LUA_JOYPAD_1P:
				return currMovieData.records[frame].joysticks[0];
			case LUA_JOYPAD_2P:
				return currMovieData.records[frame].joysticks[1];
			case LUA_JOYPAD_3P:
				return currMovieData.records[frame].joysticks[2];
			case LUA_JOYPAD_4P:
				return currMovieData.records[frame].joysticks[3];
		}
		return -1;
	} else
	{
		return -1;
	}
}

// taseditor.submitinputchange(int frame, int joypad, int input)
void TASEDITOR_LUA::submitinputchange(int frame, int joypad, int input)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		if (frame >= 0)
		{
			if (joypad == LUA_JOYPAD_COMMANDS || joypad == LUA_JOYPAD_1P || joypad == LUA_JOYPAD_2P || joypad == LUA_JOYPAD_3P || joypad == LUA_JOYPAD_4P)
			{
				PENDING_CHANGES new_change;
				new_change.type = LUA_CHANGE_TYPE_INPUTCHANGE;
				new_change.frame = frame;
				new_change.joypad = joypad;
				new_change.data = input;
				pending_changes.push_back(new_change);
			}
		}
	}
}

// taseditor.submitinsertframes(int frame, int number)
void TASEDITOR_LUA::submitinsertframes(int frame, int number)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		if (frame >= 0 && number > 0)
		{
			PENDING_CHANGES new_change;
			new_change.type = LUA_CHANGE_TYPE_INSERTFRAMES;
			new_change.frame = frame;
			new_change.joypad = 0;		// doesn't matter in TAS Editor v1.0, whole frame will be inserted
			new_change.data = number;
			pending_changes.push_back(new_change);
		}
	}
}

// taseditor.submitdeleteframes(int frame, int number)
void TASEDITOR_LUA::submitdeleteframes(int frame, int number)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		if (frame >= 0 && number > 0)
		{
			PENDING_CHANGES new_change;
			new_change.type = LUA_CHANGE_TYPE_DELETEFRAMES;
			new_change.frame = frame;
			new_change.joypad = 0;		// doesn't matter in TAS Editor v1.0, whole frame will be deleted
			new_change.data = number;
			pending_changes.push_back(new_change);
		}
	}
}

// int taseditor.applyinputchanges([string name])
int TASEDITOR_LUA::applyinputchanges(const char* name)
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		int size = pending_changes.size();
		int start_index = currMovieData.getNumRecords() - 1;
		bool InsertionDeletion_was_made = false;
		if (size)
		{
			// apply changes to current movie data
			for (int i = 0; i < size; ++i)
			{
				if (pending_changes[i].frame < start_index)
					start_index = pending_changes[i].frame;
				if (pending_changes[i].frame >= (int)currMovieData.getNumRecords())
				{
					// expand movie to fit the frame
					currMovieData.insertEmpty(-1, 1 + pending_changes[i].frame - currMovieData.getNumRecords());
					markers_manager.update();
					InsertionDeletion_was_made = true;
				}
				switch (pending_changes[i].type)
				{
					case LUA_CHANGE_TYPE_INPUTCHANGE:
					{
						switch (pending_changes[i].joypad)
						{
							case LUA_JOYPAD_COMMANDS:
								currMovieData.records[pending_changes[i].frame].commands = pending_changes[i].data;
								break;
							case LUA_JOYPAD_1P:
								currMovieData.records[pending_changes[i].frame].joysticks[0] = pending_changes[i].data;
								break;
							case LUA_JOYPAD_2P:
								currMovieData.records[pending_changes[i].frame].joysticks[1] = pending_changes[i].data;
								break;
							case LUA_JOYPAD_3P:
								currMovieData.records[pending_changes[i].frame].joysticks[2] = pending_changes[i].data;
								break;
							case LUA_JOYPAD_4P:
								currMovieData.records[pending_changes[i].frame].joysticks[3] = pending_changes[i].data;
								break;
						}
						break;
					}
					case LUA_CHANGE_TYPE_INSERTFRAMES:
					{
						InsertionDeletion_was_made = true;
						currMovieData.insertEmpty(pending_changes[i].frame, pending_changes[i].data);
						if (taseditor_config.bind_markers)
							markers_manager.insertEmpty(pending_changes[i].frame, pending_changes[i].data);
						break;
					}
					case LUA_CHANGE_TYPE_DELETEFRAMES:
					{
						InsertionDeletion_was_made = true;
						for (int t = pending_changes[i].data; t > 0; t--)
						{
							if (pending_changes[i].frame < (int)currMovieData.getNumRecords())
								currMovieData.records.erase(currMovieData.records.begin() + pending_changes[i].frame);
							if (taseditor_config.bind_markers)
								markers_manager.EraseMarker(pending_changes[i].frame);
						}
						break;
					}
				}
			}
			if (taseditor_config.bind_markers)
				selection.must_find_current_marker = playback.must_find_current_marker = true;
			// check if user deleted all frames
			if (!currMovieData.getNumRecords())
				playback.StartFromZero();
			// reduce Piano Roll
			piano_roll.UpdateItemCount();
			// check actual changes
			int result = history.RegisterLuaChanges(name, start_index, InsertionDeletion_was_made);
			if (result >= 0)
				greenzone.InvalidateAndCheck(result);
			else
				// check for special case: user deleted empty frames of the movie
				greenzone.InvalidateAndCheck(currMovieData.getNumRecords() - 1);

			pending_changes.resize(0);
			return result;
		} else
		{
			return -1;
		}
	} else
	{
		return -1;
	}
}

// taseditor.clearinputchanges()
void TASEDITOR_LUA::clearinputchanges()
{
	if (FCEUMOV_Mode(MOVIEMODE_TASEDITOR))
	{
		pending_changes.resize(0);
	}
}
// --------------------------------------------------------------------------------

