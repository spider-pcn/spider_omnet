#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(scales)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)
data$Topo <- factor(data$Topo, levels=c("sw", "sf", "lnd"), ordered=T)
#newdata <- data[ which(data$CreditType==credit),]

levels(data$Topo)

label_list <- c("PS" = "Primal Dual Approach",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP" = "Diffindo",
                 "DCTCP_qdelay" = "Spider")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP" = "#a65628",
                "DCTCP_qdelay" = "#0c2c84")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP" = 17,
                "DCTCP_qdelay" = 19)

topo_mapping <- c(
                   "sw" = "Small World",
                   "sf" = "Scale Free",
                   "lnd" = "Lightning Network")

credit_mapping <- c(
                   "uniform" = "Equal Channel Sizes",
                   "lnd" = "Lightning Sampled Sizes")


labeller_func <- function(variable,value){
            return(topo_mapping[value])
}

# add a line plot
succ_ratio_plot_uniform <- ggplot(
                        data[data$CreditType == "uniform",],
                        aes_string(x="Credit",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        facet_wrap(~ Topo, scales="free_x", labeller=labeller_func) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=0.1) +
        scale_x_continuous(expand=c(0,0), trans="log10", breaks=c(100,200,400,800,1600,3200,6400,12800)) +
        coord_cartesian(ylim=c(0,100)) +
        labs(x="Mean Channel Size (€)", y="Success Ratio (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=16) +

        # hack to convert facet_wrap into facet_grid 
        #theme(axis.title.x = element_blank()) +
        theme(plot.title = element_text(hjust = 0.5, vjust = 0, size=rel(0.8))) + 
        theme(panel.spacing = unit(2, "lines"),
              legend.box.margin=margin(-10,-10,-10,-10), legend.margin=margin(c(5,5,1,5))) +
        theme(plot.title = element_text(hjust = 0.5, vjust = -15, size=rel(0.9)),
              plot.margin=unit(c(0.25,0.41,0.25,0.25),"cm")) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(12,"points"), legend.position="top")
ggsave(paste(plot_filename,"_uniform_credit_SuccRatio.pdf", sep=""), width=12, height=3)

succ_ratio_plot_lnd <- ggplot(
                        data[data$CreditType == "lnd",],
                        aes_string(x="Credit",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        facet_wrap( ~ Topo, scales="free_x", labeller=labeller_func) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=0.08) +
        scale_x_continuous(expand=c(0,0), trans="log10", breaks=c(500,1000,2000,4000,8000,16000,32000,64000)) +
        coord_cartesian(ylim=c(0,100)) +
        labs(x="Mean Channel Size (€)", y="Success Ratio (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=16) +
        theme(panel.spacing = unit(2, "lines"),
              legend.box.margin=margin(-10,-10,-10,-10), legend.margin=margin(c(5,5,1,5))) +
        theme(plot.title = element_text(hjust = 0.5, vjust = 0, size=rel(0.8)),
             plot.margin=unit(c(0.25,0.41,0.25,0.25),"cm")) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(12,"points"), legend.position="top")
ggsave(paste(plot_filename,"_lnd_credit_SuccRatio.pdf", sep=""), width=12, height=3)

p <- plot_grid(succ_ratio_plot_uniform, succ_ratio_plot_lnd, rel_heights=c(1,0.8), nrow = 2)
ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=12, height=3)

# add a line plot
