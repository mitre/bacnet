from app.objects.secondclass.c_fact import Fact
from app.objects.secondclass.c_relationship import Relationship
from app.utility.base_parser import BaseParser
import re


WHOIS_DEVICE_RE = re.compile(r"^\s*(\d+)\s+")


class Parser(BaseParser):
    def parse(self, blob):
        relationships = []
        for match in self.line(blob):
            facts = self._parse_whois_response(match)
            if not facts:
                continue

            for mp in self.mappers:
                source = facts.get(mp.source)
                target = facts.get(mp.target)

                if mp.edge and (source == None or target == None):
                    continue

                if not mp.edge:
                    relationships.append(
                        Relationship(
                            source=Fact(mp.source, source),
                            edge=None,
                            target=Fact(mp.target, None),
                        )
                    )
                else:
                    relationships.append(
                        Relationship(
                            source=Fact(mp.source, source),
                            edge=mp.edge,
                            target=Fact(mp.target, target),
                        )
                    )
        return relationships

    @staticmethod
    def _parse_whois_response(line):
        facts = {}
        if line.strip().startswith(';') or not line.strip():
            return facts

        m = WHOIS_DEVICE_RE.match(line.strip())
        if not m:
            return facts

        device_instance = m.group(1)

        facts["bacnet.device.instance"] = device_instance
        
        return facts
