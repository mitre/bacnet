import logging

from aiohttp import web
from aiohttp_jinja2 import template

class BacnetService:
    def __init__(self, services, name, description):
        self.name = name
        self.description = description
        self.services = services

        self.data_svc = services.get('data_svc')
        self.log = logging.getLogger("bacnet_svc") 

    @template('bacnet.html')
    async def splash(self, request):
        data = await self._get_plugin_data()
        return data 
    
    async def plugin_data(self, request):
        data = await self._get_plugin_data()
        return web.json_response(data)

    async def _get_plugin_data(self):
        # Collapse abilities by their ability IDs - this fixes issue of duplicates as locate gives executor variants
        abilities = {
            a.ability_id: {
                "name"       : a.name,
                "tactic"     : a.tactic,
                "technique_id": a.technique_id,
                "technique_name": a.technique_name,
                "description": a.description.replace('\n', '<br>')  # nicer display
            }
            for a in await self.data_svc.locate('abilities')
            if await a.which_plugin() == 'bacnet'
        }
        abilities = list(abilities.values())
        return dict(name=self.name, description=self.description, abilities=abilities)
