/**
 * @file InfeasibleInitialValues.h
 * @brief Exception thrown when given Infeasible Initial Values.
 * @date jan 24, 2015
 * @author Duy-Nguyen Ta
 */



#pragma once

namespace gtsam {
/* ************************************************************************* */
/** An exception indicating that the provided initial value is infeasible */
class InfeasibleInitialValues: public ThreadsafeException<
    InfeasibleInitialValues> {
public:
  InfeasibleInitialValues() {
  }

  virtual ~InfeasibleInitialValues() throw () {
  }

  virtual const char *what() const throw () {
    if (description_.empty())
      description_ =
          "An infeasible initial value was provided for the solver.\n";
    return description_.c_str();
  }

private:
  mutable std::string description_;
};
}
