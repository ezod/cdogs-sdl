#include <cbehave/cbehave.h>

#include <collision/minkowski_hex.h>

// Stubs
const char *JoyName(const int deviceIndex)
{
	UNUSED(deviceIndex);
	return NULL;
}


FEATURE(minkowski_hex, "Minkowski Hex")
	SCENARIO("Same position")
		GIVEN("two rectangles with the same position")
			const struct vec rectPos1 = to_vector2(3, 4);
			const struct vec rectVel1 = to_vector2(-4, 5);
			const Vec2i rectSize1 = Vec2iNew(1, 3);
			const struct vec rectPos2 = rectPos1;
			const struct vec rectVel2 = to_vector2(7, -8);
			const Vec2i rectSize2 = Vec2iNew(2, 1);

		WHEN("I check for their collision")
			struct vec collide1, collide2, normal;
			const bool result = MinkowskiHexCollide(
				rectPos1, rectVel1, rectSize1,
				rectPos2, rectVel2, rectSize2,
				&collide1, &collide2, &normal);

		THEN("the result should be true");
			SHOULD_BE_TRUE(result);
		AND("the collision points should be at the start")
			SHOULD_INT_EQUAL(collide1.x, rectPos1.x);
			SHOULD_INT_EQUAL(collide1.y, rectPos1.y);
			SHOULD_INT_EQUAL(collide2.x, rectPos2.x);
			SHOULD_INT_EQUAL(collide2.y, rectPos2.y);
		AND("the normal should be (0, 0)")
			SHOULD_INT_EQUAL(normal.x, 0);
			SHOULD_INT_EQUAL(normal.y, 0);
	SCENARIO_END

	SCENARIO("No overlap, no movement")
		GIVEN("two non-overlapping rectangles with no movement")
			const struct vec rectPos1 = to_vector2(3, 4);
			const struct vec rectVel1 = vector2_zero();
			const Vec2i rectSize1 = Vec2iNew(1, 3);
			const struct vec rectPos2 = to_vector2(8, 9);
			const struct vec rectVel2 = vector2_zero();
			const Vec2i rectSize2 = Vec2iNew(2, 1);

		WHEN("I check for their collision")
			struct vec collide1, collide2, normal;
			const bool result = MinkowskiHexCollide(
				rectPos1, rectVel1, rectSize1,
				rectPos2, rectVel2, rectSize2,
				&collide1, &collide2, &normal);

		THEN("the result should be false");
			SHOULD_BE_FALSE(result);
	SCENARIO_END

	SCENARIO("Single axis single movement")
		GIVEN("two rectangles, one moving into the other")
			const struct vec rectPos1 = vector2_zero();
			const struct vec rectVel1 = to_vector2(10, 0);
			const Vec2i rectSize1 = Vec2iNew(2, 2);
			const struct vec rectPos2 = to_vector2(5, 0);
			const struct vec rectVel2 = vector2_zero();
			const Vec2i rectSize2 = Vec2iNew(2, 2);

		WHEN("I check for their collision")
			struct vec collide1, collide2, normal;
			const bool result = MinkowskiHexCollide(
				rectPos1, rectVel1, rectSize1,
				rectPos2, rectVel2, rectSize2,
				&collide1, &collide2, &normal);

		THEN("the result should be true");
			SHOULD_BE_TRUE(result);
		AND("the collision point for the moving rectangle should not overlap "
			"with the stationary rectangle")
			SHOULD_INT_EQUAL(collide1.x, 3);
			SHOULD_INT_EQUAL(collide1.y, 0);
		AND("the collision point for the stationary rectangle should be the "
			"same as its position")
			SHOULD_INT_EQUAL(collide2.x, rectPos2.x);
			SHOULD_INT_EQUAL(collide2.y, rectPos2.y);
		AND("the normal should be (-1, 0)")
			SHOULD_INT_EQUAL(normal.x, -1);
			SHOULD_INT_EQUAL(normal.y, 0);
	SCENARIO_END
FEATURE_END

CBEHAVE_RUN(
	"minkowski_hex features are:",
	TEST_FEATURE(minkowski_hex)
)
