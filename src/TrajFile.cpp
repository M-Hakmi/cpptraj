// TrajFile.cpp
#include <cstdlib>
#include <cstring>
#include "TrajFile.h"
#include "CpptrajStdio.h"

// CONSTRUCTOR
TrajFile::TrajFile() {
  //fprintf(stderr,"TrajFile Constructor.\n");
  trajfilename=NULL;
  F=NULL;
  File=NULL;
  Frames=0;
  total_read_frames=-1; 
  currentFrame=0;
  skip=0;
  start=0;
  stop=-1;
  offset=1;
  BoxType=0;
  title=NULL;
  P=NULL;
  frameskip=0;
  targetSet=0;
  seekable=0;
  debug=0;
  progress = NULL;
  hasTemperature=0;
  FrameRange=NULL;
}

// DESTRUCTOR - virtual since this class is inherited
TrajFile::~TrajFile() {
  //fprintf(stderr,"TrajFile Destructor.\n");
  if (File!=NULL) delete File;
  if (title!=NULL) free(title);
  if (FrameRange!=NULL) delete FrameRange;
  if (progress!=NULL) delete progress;
}

/*
 * TrajFile::SetTitle()
 * Set title for this trajfile.
 */
void TrajFile::SetTitle(char *titleIn) {
  size_t titleSize;

  //mprintf("DEBUG: Attempting to set title for %s: [%s]\n",trajfilename,titleIn);
  if (titleIn==NULL) return;
  titleSize = strlen(titleIn);
  //mprintf("       Title size is %i\n",titleSize);
  if (titleSize==0) {
    mprintf("Warning: TrajFile::SetTitle(): Title for %s is 0 length.\n",trajfilename);
    return;
  }
  this->title = (char*) malloc( (titleSize+1) * sizeof(char));
  if (this->title==NULL) {
    mprintf("Error: SetTitle: Could not allocate memory for title of %s.\n",trajfilename);
    return;
  }
  strcpy(this->title, titleIn);
  // Remove terminal newline if one exists
  if (this->title[titleSize-1]=='\n') this->title[titleSize-1]='\0';
}

/*
 * TrajFile::CheckBoxType()
 * Set the trajectory box type (ortho/nonortho) based on box angles.
 * Check the current box type against the associated parmfile box type.
 * Print a warning if they are different.
 */
void TrajFile::CheckBoxType(double *box) {
  // Determine orthogonal / non-orthogonal from angles
  if (box[3]==0.0 || box[4]==0.0 || box[5]==0.0)
    BoxType=0;
  else if (box[3]==90.0 && box[4]==90.0 && box[5]==90.0)
    BoxType=1;
  else
    BoxType=2;
  if (P->BoxType != BoxType) {
    mprintf("Warning: %s contains box info of type %i (beta %lf)\n",trajfilename,
            BoxType,box[4]);
    mprintf("         but associated parmfile %s has box type %i (beta %lf)\n",P->parmName, 
            P->BoxType,P->Box[4]);
    //mprintf("         Box information from trajectory will be used.\n");
  }
  if (debug>0) mprintf("    %s: Box type is %i (beta=%lf)\n",trajfilename,BoxType,box[4]);
}

/*
 * TrajFile::PrintInfo()
 * Print general trajectory information. Call TrajFile->Info for specific information.
 */
void TrajFile::PrintInfo(int showExtended) {
  mprintf("  [%s] ",trajfilename);
  this->Info();

  mprintf(", Parm %i",P->pindex);

  if (BoxType>0) mprintf(" (with box info)");

  if (showExtended==0) {
    mprintf("\n");
    return;
  }

  if (total_read_frames!=-1) {
    if (stop!=-1) 
      //mprintf(": %i-%i, %i (reading %i of %i)",start,stop,offset,total_read_frames,Frames);
      mprintf(" (reading %i of %i)",total_read_frames,Frames);
    else
      mprintf(", unknown #frames, start=%i offset=%i",start,offset);
  } else {
    mprintf(": Writing %i frames", P->parmFrames);
    if (File->access==APPEND) mprintf(", appended"); // NOTE: Dangerous if REMD
  }
  if (debug>0) mprintf(", %i atoms, Box %i, seekable %i",P->natom,BoxType,seekable);
  mprintf("\n");
}

/*
 * TrajFile::SetArgs()
 * Called after initial trajectory setup, get the start stop and offset args
 * from the argument list. Do some bounds checking.
 * For compatibility with ptraj frames start at 1. So for a traj with 10 frames:
 * cpptraj: 0 1 2 3 4 5 6 7 8 9
 *   ptraj: 1 2 3 4 5 6 7 8 9 10
 * Defaults: startArg=1, stopArg=-1, offsetArg=1
 */
void TrajFile::SetArgs(int startArg, int stopArg, int offsetArg) {

  // DEBUG
  //mprintf("DEBUG: SetArgs: Original start, stop: %i %i\n",startArg,stopArg);
 
  if (startArg!=1) {
    if (startArg<1) {
      mprintf("    Warning: %s start argument %i < 1, setting to 1.\n",trajfilename,startArg);
      start=0; // cpptraj = ptraj - 1
    } else if (Frames>=0 && startArg>Frames) {
      // If startArg==stopArg and is greater than # frames, assume we want
      // the last frame (useful when reading for reference structure).
      if (startArg==stopArg) {
        mprintf("    Warning: %s start %i > #Frames (%i), setting to last frame.\n",
                trajfilename,startArg,Frames);
        start=Frames - 1;
      } else {
        mprintf("    Warning: %s start %i > #Frames (%i), no frames will be processed.\n",
                trajfilename,startArg,Frames);
        start=startArg - 1;
      }
    } else
      start=startArg - 1;
  }

  if (stopArg!=-1) {
    if ((stopArg - 1)<start) { // cpptraj = ptraj - 1
      mprintf("    Warning: %s stop %i < start, no frames will be processed.\n",
              trajfilename,stopArg);
      //stop=stopArg+1;
      stop = start;
    } else if (Frames>=0 && stopArg>Frames) {
      mprintf("    Warning: %s stop %i >= #Frames (%i), setting to max.\n", 
              trajfilename,stopArg,Frames);
      stop=Frames;
    } else
      stop=stopArg;
  }

  if (offsetArg!=1) {
    if (offset<1) {
      mprintf("    Warning: %s offset %i < 1, setting to 1.\n",
              trajfilename,offsetArg);
      offset=1;
    } else if (stop!=-1 && offsetArg > stop - start) {
      mprintf("    Warning: %s offset %i is so large that only 1 set will be processed.\n",
              trajfilename,offsetArg);
      offset=offsetArg;
    } else
      offset=offsetArg;
  }
  if (debug>0) 
    mprintf("  [%s] Args: Start %i Stop %i  Offset %i\n",trajfilename,start,stop,offset);
}

/*
 * trajFile_setupFrameInfo()
 * For the given coordinateInfo, calculate actual frame start and stop for each
 * thread based on the requested start, stop, and offset. Also calculate the
 * starting output frame. If the file is not seekable that means we will not
 * be able to predict offsets and wont be running in parallel so set output
 * frame to -1.
 * Return the total number of frames that will be read between all threads
 * Note that the input frames start counting from 1, output starts counting from 0!
 * If called with maxFrames=-1 dont update the frame in parm file.
 */
int TrajFile::setupFrameInfo(int maxFrames, int worldrank, int worldsize) {
  int Nframes;
  int ptraj_start_frame, ptraj_end_frame;
  int traj_start_frame, traj_end_frame;
  div_t divresult;
#ifdef DEBUG
  int frame, outputFrame;
#endif

  //if (stop==-1) return -1;
  if (Frames<=0) {
    outputStart=-1;
    total_read_frames=0;
    return -1;
  }

  //mprintf("DEBUG: Calling setupFrameInfo for %s with %i %i %i\n",trajfilename,
  //        start,stop,offset);

  // Calc total frames that will be read
  // Round up
  divresult = div( (stop - start), offset);
  total_read_frames = divresult.quot;
  if (divresult.rem!=0) total_read_frames++;

  // Calc min num frames read by any given thread
  // last thread gets leftovers
  // In case of 0, last thread gets the frame
  divresult = div(total_read_frames,worldsize);
  Nframes=divresult.quot;

   // Ptraj (local) start and end frame
  ptraj_start_frame=(worldrank*Nframes);
  ptraj_end_frame=ptraj_start_frame+Nframes;
  // Last thread gets the leftovers
  if (worldrank==worldsize-1) ptraj_end_frame+=divresult.rem;

  // Actual Traj start and end frame (for seeking)
  traj_start_frame=(ptraj_start_frame*offset) + start;
  traj_end_frame=((ptraj_end_frame-1)*offset) + start;
#ifdef DEBUG
  dbgprintf("\t%s: Start %i Stop %i Offset %i Total %i maxFrames %i\n",trajfilename,
            start,stop,offset,total_read_frames,maxFrames);
  dbgprintf("\t%s: Min frames per thread is  %i\n",trajfilename,Nframes);
  dbgprintf("\t%s: Local start->end:  %i->%i\n",trajfilename,
            ptraj_start_frame, ptraj_end_frame);
  dbgprintf("\t%s: Actual start->end: %i->%i\n",trajfilename,
            traj_start_frame, traj_end_frame);
#endif
  start=traj_start_frame;
  // If the actual end frame is less than 0, indicate this thread
  // will skip processing of this trajectory
  if (traj_end_frame < 0)
      skip = 1;
  stop=traj_end_frame;

  //if (seekable)
    outputStart=ptraj_start_frame + maxFrames;
  //else
  //  outputStart=-1;

#ifdef DEBUG
  dbgprintf("\t%s: Output start %i  Skip %i\n", trajfilename,outputStart,skip);

  dbgprintf("TRAJ : ");
  for (frame=traj_start_frame; frame <= traj_end_frame; frame+=offset)
    dbgprintf("%3i,",frame);
  dbgprintf("\n");

  dbgprintf("PTRAJ: ");
  outputFrame=outputStart;
  for (frame=traj_start_frame; frame <= traj_end_frame; frame+=offset) {
    dbgprintf("%3i,",outputFrame++);
  }
  dbgprintf("\n");

  parallel_barrier(); 
#endif

  if (maxFrames!=-1)
    P->parmFrames+=total_read_frames;
 
  return total_read_frames;
}
//--------------------------------------------------------------------

/* TrajFile::Begin()
 * Prepare trajectory file for reading. Set output start frame if seekable.
 * Allocate memory for the frame.
 * Return 0 on success, 1 if traj should be skipped.
 */
int TrajFile::Begin(int *OutputStart, bool showProgressIN) {
  // Open the trajectory.
  open();
  if (showProgressIN) progress = new ProgressBar(stop);
  // If this trajectory is being skipped by this thread no setup needed
  /* NOTE: Right now the trajectory is opened even if it is skipped. Once this
   * routine exits with 1 the trajectory is immediately closed in 
   * PtrajState::Run. This is done because if trajectory reads are ever done
   * with MPI routines all threads need to call MPI_open and MPI_close, even
   * if no IO is done by the thread. 
   */
  if (skip) {
    rprintf("\tSkipping %s\n",trajfilename);
    return 1;
  }

  // Allocate frame based on this trajectory parm file. 
  //F=new Frame(P);
  F=new Frame(P->natom, P->mass);
  // Determine what frames will be read
  targetSet=start;
  if (seekable) {
    frameskip = offset;
    currentFrame = start;
    // If outputStart < 0 this means previous trajectories were not seekable.
    if (outputStart>=0) *OutputStart = outputStart;
  } else {
    frameskip = 1;
    currentFrame = 0;
  }
  rprintf( "----- [%s] (%i-%i, %i) -----\n",trajfilename,currentFrame+1,stop+1,offset);
  //mprintf("  Processing %s from frame %i, frameskip %i, OutputStart %i\n",
  //        trajfilename, currentFrame, frameskip, *OutputStart);
#ifdef DEBUG
  dbgprintf("  Processing %s from frame %i, frameskip %i, OutputStart %i, stop %i\n",
            trajfilename, currentFrame, frameskip, *OutputStart,stop);
#endif
  return 0;
}

/* TrajFile::Begin()
 * Prepare a traj file for writing. Only open.
 */
int TrajFile::Begin() {
  //rprintf("DEBUG: Preparing %s for traj write.\n",trajfilename);
  //rprintf("DEBUG: open() returned %i\n",open());
  if (open()) return 1;
  return 0;
}

/* TrajFile::NextFrame()
 * Cycle through trajectory until the next target frame. Increment the internal
 * frame counter by frameskip and the global set counter by 1.
 * Return 1 on success, 0 on failure
 */
int TrajFile::NextFrame(int *global_set) {
  int process;
  // If the current frame is out of range, exit
  if (currentFrame>stop && stop!=-1) return 0;
  if (progress!=NULL) 
    progress->Update(currentFrame);
    //progress->PrintBar(currentFrame);

  process=0;

  while ( process==0 ) {

    if (getFrame(currentFrame)) return 0;
    //printf("DEBUG:\t%s:  current=%i  target=%i\n",trajfilename,currentFrame,targetSet);
#ifdef DEBUG
    dbgprintf("DEBUG:\t%s:  current=%i  target=%i\n",trajfilename,currentFrame,targetSet);
#endif
    if (currentFrame==targetSet) {
      process=1;
      targetSet+=offset;
    }

    *global_set = *global_set + 1;
    currentFrame+=frameskip;
  }

  return 1;
}

/* TrajFile::End()
 * Close trajectory and free up the frame.
 */
void TrajFile::End() {
  close();
  if (F!=NULL) delete F;
  F=NULL;
}

