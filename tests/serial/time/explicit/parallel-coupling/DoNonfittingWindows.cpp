#ifndef PRECICE_NO_MPI

#include "testing/Testing.hpp"

#include <precice/precice.hpp>
#include <vector>

using namespace precice;

BOOST_AUTO_TEST_SUITE(Integration)
BOOST_AUTO_TEST_SUITE(Serial)
BOOST_AUTO_TEST_SUITE(Time)
BOOST_AUTO_TEST_SUITE(Explicit)
BOOST_AUTO_TEST_SUITE(SerialCoupling)

/**
 * @brief Test to ensure that correct max-time is reached, if time-window-size does not fit. See https://github.com/precice/precice/issues/1922 for context.
 *
 */
BOOST_AUTO_TEST_CASE(DoNonfittingWindows)
{
  PRECICE_TEST("SolverOne"_on(1_rank), "SolverTwo"_on(1_rank));

  Participant precice(context.name, context.config(), 0, 1);

  std::string meshName, writeDataName, readDataName;

  if (context.isNamed("SolverOne")) {
    meshName      = "MeshOne";
    writeDataName = "DataOne";
    readDataName  = "DataTwo";
  } else {
    BOOST_TEST(context.isNamed("SolverTwo"));
    meshName      = "MeshTwo";
    writeDataName = "DataTwo";
    readDataName  = "DataOne";
  }

  double writeData, readData;

  double   v0[]     = {0, 0, 0};
  VertexID vertexID = precice.setMeshVertex(meshName, v0);

  int                 timestep   = 0;
  int                 timewindow = 0;
  double              time       = 0;
  std::vector<double> expectedSizes{0.75, 0.25};

  if (precice.requiresInitialData()) {
    writeData = 1; // don't care
    precice.writeData(meshName, writeDataName, {&vertexID, 1}, {&writeData, 1});
  }

  precice.initialize();
  int nWindows = 0;

  while (precice.isCouplingOngoing()) {
    BOOST_TEST(precice.getMaxTimeStepSize() == expectedSizes[nWindows]);
    double dt = precice.getMaxTimeStepSize();
    precice.advance(dt);
    if (precice.isTimeWindowComplete()) {
      nWindows++;
    }
  }
  BOOST_TEST(nWindows == 2);

  precice.finalize();
}

BOOST_AUTO_TEST_SUITE_END() // Integration
BOOST_AUTO_TEST_SUITE_END() // Serial
BOOST_AUTO_TEST_SUITE_END() // Time
BOOST_AUTO_TEST_SUITE_END() // Explicit
BOOST_AUTO_TEST_SUITE_END() // SerialCoupling

#endif // PRECICE_NO_MPI
