from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/Game/ResCollect.c").read_text()


class SharedAsteroidHarvestingTests(unittest.TestCase):
    def test_asteroids_bypass_all_authored_and_scaled_reservation_limits(self):
        function = SOURCE.split("bool32 ResourceAlreadyBeingHarvested(", 1)[1]
        function = function.split("Resource *rescollectFindNearestResource", 1)[0]
        early = function.split("if (!resourcestaticinfo->harvestableByMultipleShips)", 1)[0]
        self.assertIn("if (resource->objtype == OBJ_AsteroidType)", early)
        self.assertIn("return FALSE;", early)
        self.assertNotIn("singlePlayerGame", early)
        self.assertNotIn("shiprace", early)
        self.assertIn("targetedby >= resourcestaticinfo->harvestableByMultipleShips", function)

    def test_direct_and_automatic_orders_share_the_same_check(self):
        process = SOURCE.split("if (ResourceAlreadyBeingHarvested(&universe.mainCommandLayer", 1)[1]
        self.assertIn("ship->ShipXHarvestsResourceY(ship,resource);", process)
        self.assertIn("rescollectFindNearestResource(ship", SOURCE)
        nearest = (ROOT / "src/Game/UnivUpdate.c").read_text().split(
            "Resource *univFindNearestResource(", 1)[1]
        self.assertIn("ResourceAlreadyBeingHarvested(&universe.mainCommandLayer,NULL,resource)", nearest)

    def test_harvesters_consume_one_shared_balance_with_cargo_clamp(self):
        collect = SOURCE.split("bool32 CollectResources(", 1)[1].split("void ModifyHarvestEffect", 1)[0]
        self.assertIn("? harvestAmount : resource->resourcevalue", collect)
        self.assertIn("eat -= (ship->resources - shipstatic->maxresources)", collect)
        self.assertIn("resource->resourcevalue -= eat", collect)
        self.assertIn("AddResourceToDeleteResourceList(resource)", collect)


if __name__ == "__main__":
    unittest.main()
