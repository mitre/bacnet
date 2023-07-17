import logging

class BacnetService:
    def __init__(self, services):
        self.services = services
        self.file_svc = services.get('file_svc')
        self.log = logging.getLogger('bacnet_svc')