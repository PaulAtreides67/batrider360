# br-manual:1 — renders the batrider360 manual (HTML/CSS -> PDF with WeasyPrint) and page previews (poppler).
FROM br-tools:1
RUN apt-get update && apt-get install -y --no-install-recommends weasyprint poppler-utils fonts-noto-cjk fonts-noto-core \
    fonts-dejavu-core fontconfig && rm -rf /var/lib/apt/lists/* && fc-cache -f
