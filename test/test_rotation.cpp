#define _USE_MATH_DEFINES
#include "gtest/gtest.h"
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <cmath>

namespace robotsthatdream {

/*

  ===== Defition of rotational matrix =====

  Rotational_matrix_OBB M is defined by: its *columns* are the
  components of the major (e1), middle (e2), minor (e3) axes.

  Properties:

  M*[1 0 0] -> e1
  M*[0 1 0] -> e2
  M*[0 0 1] -> e3

  Thus, the rotational_matrix sends X to e1, Y to e2, Z to e3.

  We say rotation matrix M rotates from frame F to frame G.

  In other words, the rotational matrix M, for any point P in space
  whose coordinates are expressed p1p2p3 in frame F, computes the
  coordinates in frame F of the point Q which has coordinates p1p2p3
  in frame G.

  Okay.

*/

/*

  ===== Definition of coordinate system: axis convention =====

  First, we have to choose a coordinate system convention.

  We choose the axis orientation convention used by the ROS project,
  relative to the body (not the camera) of the robot.
  http://www.ros.org/reps/rep-0103.html#axis-orientation

  - x forward
  - y left
  - z up

           Z

           |
         x |
          \|
    Y -----O

  To simplify things, we imagine the robot facing east.  In this case,
  body-based axes orientation matches the ROS conventions:

  > For short-range Cartesian representations of geographic locations,
  > use the east north up [5] (ENU) convention:
  >
  > - X east
  > - Y north
  > - Z up

  We'll say the "neutral" orientation of the robot is standing facing
  east.

*/

/*

  ===== Concrete illustration of axis convention =====

  Let's make things more concrete with a familiar object: a book.

  Consider a book with rigid cover in 3D.

  - Strongest moment is page height (longest dimension),

  - second moment is page width,

  - third moment is book thickness (shortest dimension).


  We'll define a "neutral" position of the book.

  - The book is lying closed on a horizontal table.

  - The top of the pages is towards east.

  - The right of the pages is towards south.

  - From front cover to back cover (drilling through the book), is
    going downward.

  This "neutral" book position is "neutral", that is rotation matrix
  is identity, rotation angles are zero.


  - From bottom of pages to top of pages, x increases.

  - From left of cover page to right of cover page, y decreases.

  - From front cover to back cover (drilling through the book), z
    decreases.

*/

/*

  ===== Definition of angle convention =====

  We have to choose an angle convention,

  ROS says:

  > fixed axis roll, pitch, yaw about X, Y, Z axes respectively
  >
  > * No ambiguity on order
  > * Used for angular velocities

  I understand it in a way that matches the Tait-Bryan convention (as
  explained on https://en.wikipedia.org/wiki/Euler_angles ).

  - First angle provides general orientation of the book as viewed
    from above.

  - Second angle corresponds to lifting the top of the page towards
    you.

  - Third angle corresponds to opening the book cover.

  These angles correspond also to "natural" ways to describe aircraft
  orientation and orientation of a camera on a tripod.


  You can now refer to illustration on
  https://en.wikipedia.org/wiki/Euler_angles#/media/File:Taitbrianzyx.svg
  oh forget that, no don't even look at it, it's awful. ;-)

  Now, if you turn the book, keeping it still lying on the table, the
  angle "yaw" will reflect that.  "Yaw" will increase from 0 to pi
  then -pi to 0 as you rotate the book counter-clockwise.

  Whatever the value of "yaw", if you lift the top of the pages, keeping
  the bottom of the pages on the table, you will increase the second
  angle, "pitch".  "Pitch" can go from -pi/2 to +pi/2.

  Whatever the value of "yaw" and "pitch", you can open the book.  The
  motion of the cover page is a decrease of "roll".  "Roll" can go from -pi/2
  to +pi/2.

*/

/*

  We need a way to transform once a rotation matrix into angles "yaw",
  "pitch", "roll": to deduce angles from the matrix found by
  pcl::MomentOfInertiaEstimation.

  Converting from angles to rotation matrix: [c++ - How to calculate
  the angle from rotation matrix - Stack
  Overflow](https://stackoverflow.com/questions/15022630/how-to-calculate-the-angle-from-rotation-matrix
  "c++ - How to calculate the angle from rotation matrix - Stack
  Overflow")

  We need a way, given angles "yaw", "pitch", "roll", to rotate a point
  cloud.  We'll follow
  http://pointclouds.org/documentation/tutorials/matrix_transform.php

*/

void matrix_to_angles(const Eigen::Matrix3f &m, float &yaw, float &pitch,
                      float &roll) {
    /* Ok, so how do we compute our angles?

       Yaw is the one we can compute first.

       It's the angle from X to its image.

       The image of X is in the first column of the matrix.

       So, we'll use only m(0,0) and m(1,0).

       Note: in case pitch is +-PI, the image of X has both X and Y
       components equal to 0.  From "man atan2":

       > If y is +0 (-0) and x is +0, +0 (-0) is returned.

       0 is an acceptable value in this case.

    */
    yaw = atan2f(m(1, 0), m(0, 0));

    /* Ok, we have found yaw.

       We can continue with "next angle" by rotating the whole matrix
       to cancel the yaw, and compute pitch.

       We can also compute pitch directly.

       Basically, the matrix send Z to a vector which Z component is
       cos(pitch).  So, we get cos(pitch) for free in m(2,2).

       If yaw = 0, the matrix sends X to a vector which Z component is
       sin(pitch).
       If yaw = PI/2, the matrix sends X to a vector which Z component is
       sin(pitch).
       Actually, whatever the yaw, the matrix sends X to a vector which Z
       component is sin(pitch).

       What is the Z component of M(X)?  M(X) is defined in column 1.  We want
       m(0,2).

       Take atan2 of those and we're done.
     */

    //std::cerr << "m(2,0)=" << m(2, 0) << ", m(2,2)=" << m(2, 2) << std::endl;

    pitch = atan2(m(2, 0), m(2, 2));

    /* Ok, we're nearly there.

       Roll sends ... ok je sors la formule de mon chapeau, adaptée
       par essai et erreur de
       https://stackoverflow.com/questions/15022630/how-to-calculate-the-angle-from-rotation-matrix

       Plus rigoureusement, le lien ci-dessus prétend que la forme est correcte.

       Il y a un petit nombre de combinaisons (en fait 2 * 2).

       Si la formule donne de bons résultats sur un grand nombre de
       cas, c'est nécessairement la bonne.

     */

    roll = atan2(m(2, 1), hypot(m(2, 0), m(2, 2)));
    // std::cerr << "roll=" << roll << std::endl;
    // roll = atan2(-m(2, 1), hypot(m(1, 0), m(0, 0)));
    // std::cerr << "roll=" << roll << std::endl;
}

/*

  Eigen offers ways to "easily" convert from angles to a rotation
  matrix:

  > Combined with MatrixBase::Unit{X,Y,Z}, AngleAxis can be used to
  easily mimic Euler-angles. Here is an example:
  > Matrix3f m;
  > m = AngleAxisf(0.25*M_PI, Vector3f::UnitX())
  >   * AngleAxisf(0.5*M_PI,  Vector3f::UnitY())
  >   * AngleAxisf(0.33*M_PI, Vector3f::UnitZ());
  > cout << m << endl << "is unitary: " << m.isUnitary() << endl;

  http://eigen.tuxfamily.org/dox/classEigen_1_1Matrix.html#a2f6bdcb76b48999cb9135b828bba4e7d

  TODO find correct convention.

*/

void angles_to_matrix(const float &yaw, const float &pitch, const float &roll,
                      Eigen::Matrix3f &m) {
    std::cerr << __PRETTY_FUNCTION__ << " yaw=" << yaw << " pitch=" << pitch << " roll=" << roll
                  << std::endl;
    m = Eigen::AngleAxisf(yaw, Eigen::Vector3f::UnitZ()) *
        Eigen::AngleAxisf(-pitch, Eigen::Vector3f::UnitY()) *
        Eigen::AngleAxisf(roll, Eigen::Vector3f::UnitX());
    //std::cerr << m << std::endl << "is unitary: " << m.isUnitary() << std::endl;
}

// The fixture for testing class RotMatToAnglesTest.
class RotMatToAnglesTest : public ::testing::Test {
  protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    Eigen::Matrix3f m;
    float pitch, yaw, roll;

    RotMatToAnglesTest() {
        std::cerr << std::endl << "========================================================================" << std::endl;
        // You can do set-up work for each test here.
    }

    ~RotMatToAnglesTest() override {
        // You can do clean-up work that doesn't throw exceptions here.
        std::cerr << "========================================================================" << std::endl << std::endl;
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override { std::cerr << m << std::endl; }

    void TearDown() override {
        std::cerr << " yaw=" << yaw << " pitch=" << pitch << " roll=" << roll
                  << std::endl;
    }

    // Objects declared here can be used by all tests in the test case for
    // RotMatToAnglesTest.
};

TEST_F(RotMatToAnglesTest, EigenTest_CoeffAccessIsMRowColumn) {
    m.row(0) << 1, 2, 3;
    m.row(1) << 4, 5, 6;
    m.row(2) << 7, 8, 9;

    EXPECT_EQ(m(0, 0), 1);
    EXPECT_EQ(m(1, 0), 4);
    EXPECT_EQ(m(2, 0), 7);
    EXPECT_EQ(m(2, 1), 8);
    EXPECT_EQ(m(2, 2), 9);
}

TEST_F(RotMatToAnglesTest, Identity) {
    m.row(0) << 1, 0, 0;
    m.row(1) << 0, 1, 0;
    m.row(2) << 0, 0, 1;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, 0, 1e-5);
    EXPECT_NEAR(pitch, 0, 1e-5);
    EXPECT_NEAR(roll, 0, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       YawTest_RotateBookCounterClockwiseQuarterTurnMustYieldYawPi2) {
    // This matrix sends X to Y.
    // This matrix sends Y to -X.
    // This matrix sends Z to Z.
    m.row(0) << 0, -1, 0;
    m.row(1) << 1, 0, 0;
    m.row(2) << 0, 0, 1;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, M_PI_2, 1e-5);
    EXPECT_NEAR(pitch, 0, 1e-5);
    EXPECT_NEAR(roll, 0, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       YawTest_RotateBookCounterClockwiseEigthTurnMustYieldYawPi4) {
    // This matrix sends X to Y.
    // This matrix sends Y to -X.
    // This matrix sends Z to Z.
    m.row(0) << M_SQRT2, -M_SQRT2, 0;
    m.row(1) << M_SQRT2, M_SQRT2, 0;
    m.row(2) << 0, 0, 1;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, M_PI_4, 1e-5);
    EXPECT_NEAR(pitch, 0, 1e-5);
    EXPECT_NEAR(roll, 0, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       PitchTest_LiftBookPagetopQuarterTurnMustYieldPitchPi2) {
    // This matrix sends X to Z.
    // This matrix sends Y to Y.
    // This matrix sends Z to -X.
    m.row(0) << 0, 0, -1;
    m.row(1) << 0, 1, 0;
    m.row(2) << 1, 0, 0;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, 0, 1e-5);
    EXPECT_NEAR(pitch, M_PI_2, 1e-5);
    EXPECT_NEAR(roll, 0, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       PitchTest_LiftBookPagetopEighthTurnMustYieldPitchPi4) {
    // This matrix sends X to Z.
    // This matrix sends Y to Y.
    // This matrix sends Z to -X.
    m.row(0) << M_SQRT2, 0, -M_SQRT2;
    m.row(1) << 0, 1, 0;
    m.row(2) << M_SQRT2, 0, M_SQRT2;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, 0, 1e-5);
    EXPECT_NEAR(pitch, M_PI_4, 1e-5);
    EXPECT_NEAR(roll, 0, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       PitchTest_OpenBookCoverQuarterTurnMustYieldRollMinusPi2) {
    // This matrix sends X to X.
    // This matrix sends Y to -Z.
    // This matrix sends Z to Y.
    m.row(0) << 1, 0, 0;
    m.row(1) << 0, 0, 1;
    m.row(2) << 0, -1, 0;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, 0, 1e-5);
    EXPECT_NEAR(pitch, 0, 1e-5);
    EXPECT_NEAR(roll, -M_PI_2, 1e-5);
}

TEST_F(RotMatToAnglesTest,
       PitchTest_OpenBookCoverEighthTurnMustYieldRollMinusPi4) {
    // This matrix sends X to X.
    // This matrix sends Y to -Z.
    // This matrix sends Z to Y.
    m.row(0) << 1, 0, 0;
    m.row(1) << 0, M_SQRT2, M_SQRT2;
    m.row(2) << 0, -M_SQRT2, M_SQRT2;

    matrix_to_angles(m, yaw, pitch, roll);

    EXPECT_NEAR(yaw, 0, 1e-5);
    EXPECT_NEAR(pitch, 0, 1e-5);
    EXPECT_NEAR(roll, -M_PI_4, 1e-5);
}

TEST_F(RotMatToAnglesTest, PitchTest_AnyComboMustConvertAndBack) {

    const float increment = 1;

    for (float orig_yaw = 0; orig_yaw < M_PI; orig_yaw += increment) {
        for (float orig_pitch = 0; orig_pitch < 1.5;
             orig_pitch += increment) {
            for (float orig_roll = 0; orig_roll < 1.5;
                 orig_roll += increment) {
                
                angles_to_matrix(orig_yaw, orig_pitch, orig_roll, m);

                matrix_to_angles(m, yaw, pitch, roll);

                EXPECT_NEAR(yaw, orig_yaw, 1e-5);
                EXPECT_NEAR(pitch, orig_pitch, 1e-5);
                EXPECT_NEAR(roll, orig_roll, 1e-5);
            }
        }
    }
}
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
