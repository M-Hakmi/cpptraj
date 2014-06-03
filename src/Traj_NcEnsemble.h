#ifndef INC_TRAJ_NCENSEMBLE_H
#define INC_TRAJ_NCENSEMBLE_H
#ifdef BINTRAJ
#include "TrajectoryIO.h"
#include "NetcdfFile.h"
/// Read/write single NetCDF ensemble trajectory.
class Traj_NcEnsemble : public TrajectoryIO, private NetcdfFile {
  public:
    Traj_NcEnsemble();
    ~Traj_NcEnsemble();
    static BaseIOtype* Alloc() { return (BaseIOtype*)new Traj_NcEnsemble(); }
    // Inherited functions
    bool ID_TrajFormat(CpptrajFile&);
    int setupTrajin(std::string const&, Topology*);
    int setupTrajout(std::string const&, Topology*, int, bool);
    int openTrajin();
    void closeTraj();
    int readFrame(int,Frame&);
    int readVelocity(int, Frame&);
    int writeFrame(int,Frame const&);
    void Info();
    int processWriteArgs(ArgList&);
    int processReadArgs(ArgList&);
    bool CanProcessEnsemble() { return true; }
    int writeArray(int, FramePtrArray const&);
  private:
    float *Coord_;
    FileName filename_;
    int ensembleSize_;
    bool readAccess_;
};
#endif
#endif
