#!/usr/bin/env Rscript

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)

args <- commandArgs(trailingOnly=TRUE)
plot_filename <- args[1]
d <- read.csv("toy_example_timeseries_data", sep=",")
queue_data <- d[ which(d$statistic=='queue' & d$scenario == plot_filename & d$time <= 800), ]
rate_data <- d[ which(d$statistic=='rate' & d$scenario == plot_filename  & d$time <= 800), ]

queue_plot <- ggplot(queue_data, aes(x=time,y=value, colour=router)) +
	geom_line(size=0.7) + 
    facet_wrap(~ router) +
    ylim(0, 20) +
    xlab("Time (s)") + ylab("Queue Size (EUR)") +
    scale_colour_brewer(type="qual", palette=2, guide=guide_legend(title=NULL)) +
    theme_minimal(base_size=15) +
    theme(
	legend.text=element_text(size=rel(0.8)), 
	legend.key.size=unit(10,"points"),
	legend.position="top", 
	legend.margin=margin(c(0,5,1,5)), 
	strip.background = element_blank(), 
	strip.text.x = element_blank())

# just to get legend from it that doesn't show line type
point_plot <- ggplot(rate_data, aes(x=time,y=value, colour=router)) +
	geom_point(size=3) + 
    facet_wrap(~ router) +
    xlab("Time (s)") + ylab("Sending Rate (EUR/s)") +
    scale_colour_brewer(type="qual", palette=2, guide=guide_legend(title=NULL)) +
    theme_minimal(base_size=22) +
    guides(linetype=FALSE) +
    theme(legend.position="top", 
	legend.margin=margin(c(0,5,1,5)), 
	strip.background = element_blank(), 
	strip.text.x = element_blank())
legend <- get_legend(point_plot + theme(legend.position="top"))


rate_plot <- ggplot(rate_data, aes(x=time,y=value, colour=router)) +
	geom_line(size=0.7, linetype="twodash") + # dotted lines 
    facet_wrap(~ router) +
    xlab("Time (s)") + ylab("Sending Rate (EUR/s)") +
    scale_colour_brewer(type="qual", palette=2, guide=guide_legend(title=NULL)) +
    theme_minimal(base_size=15) +
    guides(linetype=FALSE) +
    theme(legend.position="top", 
	legend.margin=margin(c(0,5,1,5)), 
	strip.background = element_blank(), 
	strip.text.x = element_blank())


prow <- plot_grid(rate_plot + theme(legend.position="none"),
                  queue_plot + theme(legend.position="none",legend.title=element_blank()), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"RateQueue.pdf", sep=""), width=12.2, height=3)
