-- =========================
-- Warehouses Functionality 
-- =========================
warehouse_tests = lunit.TestCase("warehouse tests")
function warehouse_tests:setup()
   self.f = wl.map.Field(10,10)
   self.p = wl.game.Player(1)
   self.w = self.p:place_building("headquarters", self.f)
end
function warehouse_tests:teardown()
   pcall(self.f.brn.remove, self.w)
end

function warehouse_tests:test_upcasting_from_immovable_to_building()
   i = self.f.immovable
   assert_equal(i, self.w)
   assert_function(i.set_wares) -- set_wares is non nil
end

function warehouse_tests:test_set_ware_illegal_ware()
   function ill()
      self.w:set_wares("sdjsgfhg", 100)
   end
   assert_error("Illegal ware should throw an error!", ill)
end
function warehouse_tests:test_set_get_wares_string_arg()
   assert_equal(0, self.w:get_wares("trunk"))
   self.w:set_wares("trunk", 190)
   assert_equal(190, self.w:get_wares("trunk"))
   assert_equal(0, self.w:get_wares("raw_stone"))
end
function warehouse_tests:test_set_get_wares_all()
   self.w:set_wares{trunk=190, raw_stone=170}
   local rv = self.w:get_wares("all")
   assert_equal(190, rv.trunk)
   assert_equal(170, rv.raw_stone)
   assert_equal(0, rv.coal)
end
function warehouse_tests:test_set_get_wares_table_arg()
   k = self.w:get_wares{"trunk", "raw_stone"}
   assert_equal(0, k.trunk)
   assert_equal(0, k.raw_stone)
   assert_equal(nil, k.coal)
   self.w:set_wares{trunk=190, raw_stone=170}
   k = self.w:get_wares{"trunk", "raw_stone"}
   assert_equal(190, k.trunk)
   assert_equal(170, k.raw_stone)
   assert_equal(nil, k.coal)
end
function warehouse_tests:test_set_get_wares_set_is_not_increase()
   k = self.w:get_wares{"trunk", "raw_stone"}
   k.trunk = 20
   k.raw_stone = 40
   self.w:set_wares(k)
   k = self.w:get_wares{"trunk", "raw_stone"}
   assert_equal(20, k.trunk)
   assert_equal(40, k.raw_stone)

   k.trunk = 10
   k.raw_stone = 20
   self.w:set_wares(k)
   k = self.w:get_wares{"trunk", "raw_stone"}
   assert_equal(10, k.trunk)
   assert_equal(20, k.raw_stone)
end
function warehouse_tests:test_get_wares_non_existant_name()
   assert_error("non existent ware", function() 
      self.w:get_wares("balloon")
   end)
   assert_error("non existent ware", function() 
      self.w:get_wares{"meat", "balloon"}
   end)
end

function warehouse_tests:test_set_worker_illegal_worker()
   function ill()
      self.w:set_workers("sdjsgfhg", 100)
   end
   assert_error("Illegal worker should throw an error!", ill)
end
function warehouse_tests:test_set_get_workers_string_arg()
   assert_equal(0, self.w:get_workers("builder"))
   self.w:set_workers("builder", 190)
   assert_equal(190, self.w:get_workers("builder"))
end
function warehouse_tests:test_set_get_workers_table_arg()
   k = self.w:get_workers{"builder", "lumberjack"}
   assert_equal(0, k.builder)
   assert_equal(0, k.lumberjack)
   self.w:set_workers{builder=190, lumberjack=170}
   k = self.w:get_workers{"builder", "lumberjack"}
   assert_equal(190, k.builder)
   assert_equal(170, k.lumberjack)
end
function warehouse_tests:test_set_get_workers_set_is_not_increase()
   k = self.w:get_workers{"builder", "lumberjack"}
   k.builder = 20
   k.lumberjack = 40
   self.w:set_workers(k)
   k = self.w:get_workers{"builder", "lumberjack"}
   assert_equal(20, k.builder)
   assert_equal(40, k.lumberjack)

   k.builder = 10
   k.lumberjack = 20
   self.w:set_workers(k)
   k = self.w:get_workers{"builder", "lumberjack"}
   assert_equal(10, k.builder)
   assert_equal(20, k.lumberjack)
end


-- =========
-- Soldiers 
-- =========
function warehouse_tests:test_set_soldiers()
   self.w:set_soldiers({0,0,0,0}, 100)
   -- TODO: there is no way to verify this ATM
end
function warehouse_tests:test_set_soldiers_by_list()
   self.w:set_soldiers{
      [{0,0,0,0}] = 1,
      [{1,1,0,1}] = 2,
   }
   -- TODO: there is no way to verify this ATM
end
function warehouse_tests:test_set_soldiers_hp_too_high()
   assert_error("hp too high", function()
      self.w:set_soldiers({10,0,0,0}, 1)
   end)
end
function warehouse_tests:test_set_soldiers_attack_too_high()
   assert_error("attack too high", function()
      self.w:set_soldiers({0,10,0,0}, 1)
   end)
end
function warehouse_tests:test_set_soldiers_defense_too_high()
   assert_error("defense too high", function()
      self.w:set_soldiers({0,0,10,0}, 1)
   end)
end
function warehouse_tests:test_set_soldiers_evade_too_high()
   assert_error("evade too high", function()
      self.w:set_soldiers({0,0,0,10}, 1)
   end)
end


