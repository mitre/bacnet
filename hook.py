from app.utility.base_world import BaseWorld
from plugins.bacnet.app.bacnet_gui import BacnetGUI
from plugins.bacnet.app.bacnet_api import BacnetAPI

name = 'BACnet'
description = 'The BACnet plugin for Caldera provides adversary emulation abilities specific to the BACnet control systems protocol.'
address = '/plugin/bacnet/gui'
access = BaseWorld.Access.RED


async def enable(services):
    app = services.get('app_svc').application
    bacnet_gui = BacnetGUI(services, name=name, description=description)
    app.router.add_static('/bacnet', 'plugins/bacnet/static/', append_version=True)
    app.router.add_route('GET', '/plugin/bacnet/gui', bacnet_gui.splash)

    bacnet_api = BacnetAPI(services)
    # Add API routes here
    app.router.add_route('POST', '/plugin/bacnet/mirror', bacnet_api.mirror)

