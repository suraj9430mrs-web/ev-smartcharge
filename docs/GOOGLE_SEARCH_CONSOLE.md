# EV SmartCharge — Google Search Console Setup

## 1. Verify Domain Ownership

1. Go to https://search.google.com/search-console
2. Click **Add Property**
3. Choose **URL prefix** → Enter your domain (e.g., `https://evsmartcharge.in`)
4. Choose a verification method:
   - **HTML tag** (recommended): Add the provided `<meta>` tag to `frontend/index.html` in the `<head>` section
   - **HTML file**: Download the verification file and place it in `frontend/`
5. Click **Verify**

## 2. Submit Sitemap

1. In Search Console, go to **Sitemaps** (left sidebar)
2. Enter: `sitemap.xml`
3. Click **Submit**
4. Google will crawl and index your public pages

## 3. Monitor Indexing

After submission, Google typically indexes pages within 2–7 days. Check:
- **Coverage** — shows indexed vs excluded pages
- **Performance** — search queries and click-through rates
- **Mobile Usability** — responsive design validation

## 4. SEO Meta Tags

The following meta tags are already configured in the public pages:

| Page | Title | Description |
|------|-------|-------------|
| Home | EV SmartCharge — Power Your Journey | India's premier smart EV charging network |
| Find a Charger | Find Charging Stations | Search compatible EV charging stations |
| Solutions | Charging Solutions | Business and fleet charging solutions |
| About | About EV SmartCharge | Company mission and team |
| Contact | Contact Us | Get in touch with our team |

## 5. Robots.txt

The `robots.txt` file at `/robots.txt` is configured to:
- **Allow** all public pages (Home, Find a Charger, Solutions, About, Contact)
- **Disallow** private pages (Admin, Dashboard, Login, Register, API endpoints)

## Notes

- Google Search Console requires your site to be publicly accessible on the internet
- Local development (`localhost:8080`) cannot be verified
- Deploy to a public URL first (see [DEPLOYMENT_GUIDE.md](DEPLOYMENT_GUIDE.md))

