#pragma once

#include <map>
#include <string>
#include <vector>

#include "com/SharedPointer.hpp"
#include "precice/types.hpp"

namespace precice {
namespace cplscheme {

/**
 * @brief Interface for all coupling schemes.
 *
 * ! General description
 * A coupling scheme computes the actions to be done by the coupled participants
 * (solvers) in time. It provides interface functions to setup, advance and
 * shutdown the coupling scheme and interface functions to query the state of
 * the coupling scheme and required actions of the participants.
 *
 * ! Usage
 * -# create an object of a concrete coupling scheme class
 *    (ExplicitCouplingScheme, e.g.)
 * -# add all meshes holding data to the coupling scheme by addMesh()
 * -# configure the object by adding subclass specific information
 * -# start the coupling scheme with initialize(), where the name of the local
 *    participant, i.e. the participant using the coupling scheme object, is
 *    needed
 * -# retrieve necessary information about sent/received data and the state of
 *    the coupled simulation
 * -# query and fulfill required actions
 * -# compute data to be sent (possibly taking into account received data from
 *    initialize())
 * -# advance the coupling scheme with advance(); where the maximum timestep
 *    length (= time window size) needs to be obeyed
 * -# ....
 * -# when the method isCouplingOngoing() returns false, call finalize() to
 *    stop the coupling scheme
 */
class CouplingScheme {
public:
  /// Does not define a time limit for the coupled simulation.
  static const double UNDEFINED_TIME;

  /// Does not define limit on time windows for the coupled simulation.
  static const int UNDEFINED_TIME_WINDOWS;

  /// To be used, when the time window size is determined dynamically during the coupling.
  static const double UNDEFINED_TIME_WINDOW_SIZE;

  /// To be used, when the extrapolation order is not defined (for explicit coupling).
  static const int UNDEFINED_EXTRAPOLATION_ORDER;

  /// To be used, when the number of max iterations is not defined (for explicit coupling).
  static const int UNDEFINED_MAX_ITERATIONS;

  CouplingScheme &operator=(CouplingScheme &&) = delete;

  virtual ~CouplingScheme() {}

  /**
   * @brief Initializes the coupling scheme and establishes a communication
   *        connection to the coupling partner. Initializes coupling data.
   *
   * @param[in] startTime starting time for coupling @BU correct?
   * @param[in] startTimeWindow counter of time window for coupling @BU correct?
   */
  virtual void initialize(
      double startTime,
      int    startTimeWindow) = 0;

  /**
   * @brief Receives result of first advance, if this has to happen inside SolverInterface::initialize()
   *
   * This is only relevant for the second participant of the SerialCouplingScheme, because other coupling schemes only
   * receive initial data in initialize. This part is implemented as a public function to be called from
   * SolverInterfaceImpl. SolverInterfaceImpl has to store data received in CouplingScheme::initialize before calling
   * CouplingScheme::receiveResultOfFirstAdvance, which will override the data in the receive buffer.
   */
  virtual void receiveResultOfFirstAdvance() = 0;

  /**
   * @brief Returns whether this participant of the coupling scheme sends initialized data.
   *
   * @returns true, if this participant of the coupling scheme sends initialized data
   */
  virtual bool sendsInitializedData() const = 0;

  /// Returns true, if initialize has been called.
  virtual bool isInitialized() const = 0;

  /// @brief Adds newly computed time. Has to be called before every advance.
  virtual void addComputedTime(double timeToAdd) = 0;

  /**
   * @brief Exchanges data and updates the state of the coupling scheme.
   *
   * @pre initialize() has been called.
   *
   * Does not necessarily advance in time.
   */
  void advance()
  {
    firstSynchronization({});
    firstExchange();
    secondSynchronization();
    secondExchange();
  }

  using ChangedMeshes = std::vector<MeshID>;

  /** Synchronizes mesh changes with remote participants.
   *
   * @param[in] changes MeshIDs of locally changed meshes
   *
   * @returns MeshIDs of remotely changed meshes.
   */
  virtual ChangedMeshes firstSynchronization(const ChangedMeshes &changes) = 0;

  /** Exchanges the first set of data.
   *
   * @pre \ref firstSynchronization() was called
   */
  virtual void firstExchange() = 0;

  /** Receive mesh changes from remote participants in the second step.
   * This is necessary as serial coupling schemes may have changed the mesh by now.
   *
   * @note local changes are covered by \ref firstSynchronization()
   *
   * @returns MeshIDs of remotely changed meshes.
   *
   * @pre \ref firstExchange() was called
   */
  virtual ChangedMeshes secondSynchronization() = 0;

  /** Exchanges the second set of data.
   *
   * If the step isn't done, this function returns false.
   * This requires the frist Synchronization to be called again.
   *
   * @returns true if the step is done.
   *
   * @pre \ref secondSynchronization() was called
   */
  virtual void secondExchange() = 0;

  /// Finalizes the coupling and disconnects communication.
  virtual void finalize() = 0;

  /// Returns list of all coupling partners.
  virtual std::vector<std::string> getCouplingPartners() const = 0;

  /**
   * @brief Returns true, if data will be exchanged when calling advance().
   *
   * Also returns true after the last call of advance() at the end of the
   * simulation.
   *
   * @param lastSolverTimestepLength [IN] The length of the last timestep
   *        computed by the solver calling willDataBeExchanged().
   */
  virtual bool willDataBeExchanged(double lastSolverTimestepLength) const = 0;

  /// @brief Returns true, if data has been exchanged in last call of advance().
  virtual bool hasDataBeenReceived() const = 0;

  /// Returns the currently computed time of the coupling scheme.
  virtual double getTime() const = 0;

  /// Returns the currently computed time windows of the coupling scheme.
  virtual int getTimeWindows() const = 0;

  /// Returns true, if time window size is prescribed by the cpl scheme.
  virtual bool hasTimeWindowSize() const = 0;

  /**
   * @brief Returns the time window size, if one is given by the coupling scheme.
   *
   * An assertion is thrown, if no valid time window size is given. Check with
   * hasTimeWindowSize().
   */
  virtual double getTimeWindowSize() const = 0;

  /**
   * @brief Returns the remaining time within the current time window.
   *
   * This is not necessarily the time window size limit the solver has to obey
   * which is returned by getNextTimestepMaxLength().  // TODO explain this better
   *
   * If no time window size is prescribed by the coupling scheme, always 0.0 is
   * returned.
   */
  virtual double getThisTimeWindowRemainder() const = 0;

  /**
   * @brief Returns the maximal length of the next timestep to be computed.
   *
   * If no time window size is prescribed by the coupling scheme, always the
   * maximal double accuracy floating point number value is returned.
   */
  virtual double getNextTimestepMaxLength() const = 0;

  /// Returns true, when the coupled simulation is still ongoing.
  virtual bool isCouplingOngoing() const = 0;

  /// Returns true, when the accessor can advance to the next time window.
  virtual bool isTimeWindowComplete() const = 0;

  /// Returns true, if the given action has to be performed by the accessor.
  virtual bool isActionRequired(const std::string &actionName) const = 0;

  /// Tells the coupling scheme that the accessor has performed the given action.
  virtual void markActionFulfilled(const std::string &actionName) = 0;

  /// Sets an action required to be performed by the accessor.
  virtual void requireAction(const std::string &actionName) = 0;

  /// Returns a string representation of the current coupling state.
  virtual std::string printCouplingState() const = 0;

  /// Returns true if the scheme or one subscheme is implicit
  virtual bool isImplicitCouplingScheme() const = 0;

  /// Returns false if the scheme is implicit and hasnt converged
  virtual bool hasConverged() const = 0;
};

} // namespace cplscheme
} // namespace precice
