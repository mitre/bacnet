from app.utility.base_world import BaseWorld
from plugins.bacnet.app.bacnet_svc import BacnetService 

name = 'BACnet'
description = 'The BACnet plugin for Caldera provides adversary emulation abilities specific to the BACnet control systems protocol.'
address = '/plugin/bacnet/gui'
access = BaseWorld.Access.RED


async def enable(services):
    bacnet_svc = BacnetService(services, name, description)
    app = services.get('app_svc').application
    app.router.add_route('GET', '/plugin/bacnet/gui', bacnet_svc.splash)
    app.router.add_route('GET', '/plugin/bacnet/data', bacnet_svc.plugin_data)
    