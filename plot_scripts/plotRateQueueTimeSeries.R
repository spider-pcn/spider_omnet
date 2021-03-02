#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
data_file <- args[1]
plot_filename <- args[2]
data <- read.csv(data_file)
rate_data <- data[(data$Stat == "rate"),]
queue_data<- data[(data$Stat == "queue"),]


rate_label_list <- c("endhost a" = bquote(e[a]*''%->%''*e[c]),
                     "endhost c" = bquote(e[c]*''%->%''*e[a]),
                     "endhost b" = bquote(e[b]*''%->%''*e[d]))


rate_color_list <- c(
                "endhost c" = "#33a02c",
                "endhost a" = "#ff0000",
                "endhost b" = "#0000ff")


queue_label_list <- c("router a" = bquote(q[a*''%->%''*c]),
                "router c" = bquote(q[c*''%->%''*a]))

queue_color_list <- c(
                "router a" = "#d95f02",
                "router c" = "#984ea3")

# add a line plot
rate_plot <- ggplot(rate_data, aes_string(x="Time",y="Value",color="Id")) +
        geom_line(size=1) +
        labs(x="Time (s)", y="Rate (€/s)") +
        scale_colour_manual(
                values=rate_color_list,
                labels=rate_label_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        theme_minimal(base_size=25) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(20,"points"),
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)),
              legend.position="bottom")

ggsave(paste(plot_filename,"Rate.pdf", sep=""), width=4.3, height=3.2)

legend <- get_legend(rate_plot + theme(legend.position="top"))

# add a line plot
queue_plot <- ggplot(queue_data, aes(x=Time,y=Value,color=Id)) +
        geom_line(size=1) +
        labs(x="Time (s)", y="Queue Size (€)") +
        scale_colour_manual(
                values=queue_color_list,
                labels=queue_label_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        theme_minimal(base_size=25) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(20,"points"), 
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)),
              legend.position="bottom", legend.spacing.x=unit(0.3, "cm"))

ggsave(paste(plot_filename,"Queue.pdf", sep=""), width=4.3, height=3.2)

prow <- plot_grid(rate_plot + theme(legend.position="top"),
                  queue_plot + theme(legend.position="top",legend.title=element_blank()), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"RateQueue.pdf", sep=""), width=12.2, height=3.5)
