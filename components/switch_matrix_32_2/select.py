import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import select
from esphome.const import CONF_ID
from . import Matrix, ns

MatrixSelect = ns.class_("MatrixSelect", select.Select)
CONFIG_SCHEMA = select.select_schema(MatrixSelect).extend({
    cv.Required("matrix_id"): cv.use_id(Matrix),
    cv.Required("channel"): cv.int_range(min=1, max=32),
})


def final_validate(config):
    entries = fv.full_config.get().get("select", [])
    for other in entries:
        if (other.get("matrix_id") == config["matrix_id"]
                and other.get("channel") == config["channel"]
                and other.get(CONF_ID) != config[CONF_ID]):
            raise cv.Invalid("Only one select is allowed per matrix channel")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await select.register_select(var, config, options=["A", "B", "BYPASS"])
    parent = await cg.get_variable(config["matrix_id"])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config["channel"]))
    cg.add(parent.register_select(var))
